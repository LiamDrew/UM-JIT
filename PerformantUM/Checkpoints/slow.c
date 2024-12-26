#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>

#define NUM_REGISTERS 8
#define POWER ((uint64_t)1 << 32)
#define NUM_INSTRS 1

typedef uint32_t Instruction;

uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb, uint64_t value)
{
    uint64_t mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;
    mask = mask << lsb;
    mask = ~mask;

    uint64_t new_word = (word & mask);
    value = value << lsb;
    uint64_t return_word = (new_word | value);
    return return_word;
}

uint32_t *initialize_memory(FILE *fp, size_t fsize)
{
    uint32_t *temp = (uint32_t *)calloc(fsize, sizeof(uint32_t));
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;

    for (c = getc(fp); c != EOF; c = getc(fp))
    {
        c_char = (unsigned char)c;
        if (i % 4 == 0)
            word = Bitpack_newu(word, 8, 24, c_char);
        else if (i % 4 == 1)
            word = Bitpack_newu(word, 8, 16, c_char);
        else if (i % 4 == 2)
            word = Bitpack_newu(word, 8, 8, c_char);
        else if (i % 4 == 3)
        {
            word = Bitpack_newu(word, 8, 0, c_char);
            temp[i / 4] = word;
            word = 0;
        }
        i++;
    }

    fclose(fp);
    return temp;
}

/* INSTRUCTION FETCHING */

void handle_halt(uint32_t ***segs, uint32_t *seg_size, uint32_t **seg_lens, uint32_t **rec_ids)
{
    for (uint32_t i = 0; i < *seg_size; i++)
        free((*segs)[i]);
    free(*segs);
    free(*seg_lens);
    free(*rec_ids);
    exit(EXIT_SUCCESS);
}

uint32_t map_segment(uint32_t size, uint32_t ***segs, uint32_t *seg_size,
                     uint32_t *seg_cap, uint32_t **seg_lens,
                     uint32_t *rec_ids, uint32_t *rec_size)
{
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (*rec_size == 0)
    {
        if (*seg_size == *seg_cap)
        { // expand if necessary
            *seg_cap = *seg_cap * 2 + 2;
            assert(*seg_lens != NULL);
            assert(*segs != NULL);
            *seg_lens = (uint32_t *)realloc(*seg_lens, *seg_cap * sizeof(uint32_t));
            *segs = (uint32_t **)realloc(*segs, *seg_cap * sizeof(uint32_t *));

            for (uint32_t i = *seg_size; i < *seg_cap; i++)
            {
                (*segs)[i] = NULL;
                (*seg_lens)[i] = 0;
            }
        }

        new_seg_id = (*seg_size)++;
    }

    /* Otherwise, reuse an old one */

    else
        new_seg_id = rec_ids[--(*rec_size)];

    // TODO: something is wrong in memory here. I will try to patch it together first
    // but there is a deeper issue.
    // if ((*segs)[new_seg_id] == NULL || size > (*seg_lens)[new_seg_id]) {
    //     assert((*segs)[new_seg_id] == NULL);
    //     (*segs)[new_seg_id] = (uint32_t *)realloc((*segs)[new_seg_id], size * sizeof(uint32_t));
    //     (*seg_lens)[new_seg_id] = size;
    // }

    if ((*segs)[new_seg_id] == NULL)
    {
        // assert(false);
        // (*segs)[new_seg_id] = (uint32_t *)realloc((*segs)[new_seg_id], size * sizeof(uint32_t));
        (*segs)[new_seg_id] = (uint32_t *)calloc(size, sizeof(uint32_t));
        (*seg_lens)[new_seg_id] = size;
    }

    else if (size > (*seg_lens)[new_seg_id])
    {
        assert(false);
        (*segs)[new_seg_id] = (uint32_t *)realloc((*segs)[new_seg_id], size * sizeof(uint32_t));
        (*seg_lens)[new_seg_id] = size;
    }

    memset((*segs)[new_seg_id], 0, size * sizeof(uint32_t));
    return new_seg_id;
}

void unmap_segment(uint32_t segment, uint32_t **rec_ids, uint32_t *rec_size, uint32_t *rec_cap)
{
    if (*rec_size == *rec_cap)
    {
        *rec_cap = *rec_cap * 2 + 2;
        *rec_ids = (uint32_t *)realloc(*rec_ids, *rec_cap * sizeof(uint32_t));
    }
    *rec_ids[*rec_size++] = segment;
}

void load_segment(uint32_t index, uint32_t *zero, uint32_t **segs, uint32_t *seg_lens)
{
    if (index > 0)
    {
        uint32_t copied_seq_size = seg_lens[index];
        memcpy(zero, segs[index], copied_seq_size * sizeof(uint32_t));
    }
}

Instruction get_instr(Instruction *cache, unsigned index)
{
    return cache[index];
}

uint32_t decode_instruction(uint32_t word, uint32_t *a, uint32_t *b,
                            uint32_t *c, uint32_t *val)
{
    uint32_t opcode = (word >> 28) & 0xF;
    if (opcode == 13)
    {
        *a = (word >> 25) & 0x7;
        *val = word & 0x1FFFFFF;
    }
    else
    {
        *c = word & 0x7;
        *b = (word >> 3) & 0x7;
        *a = (word >> 6) & 0x7;
    }
    return opcode;
}

void empty_cache(Instruction *cache, Instruction **pp, uint32_t *regs,
                 uint32_t *zero, uint32_t ***segs, uint32_t **seg_lens,
                 uint32_t *seg_size, uint32_t *seg_cap,
                 uint32_t **rec_ids, uint32_t *rec_size, uint32_t *rec_cap)
{
    Instruction word;
    uint32_t a = 0, b = 0, c = 0, val = 0;

    for (unsigned i = 0; i < NUM_INSTRS; i++)
    {
        word = get_instr(cache, i);
        uint32_t opcode = decode_instruction(word, &a, &b, &c, &val);

        /* Load Value */
        if (opcode == 13)
            regs[a] = val;

        /* Segmented Load (Memory Module) */
        else if (opcode == 1)
            regs[a] = (*segs)[regs[b]][regs[c]];

        /* Segmented Store (Memory Module) */
        else if (opcode == 2)
            (*segs)[regs[a]][regs[b]] = regs[c];

        /* Bitwise NAND */
        else if (opcode == 6)
            regs[a] = ~(regs[b] & regs[c]);

        /* Load Segment (Memory Module) */
        else if (opcode == 12)
        {
            load_segment(regs[b], zero, *segs, *seg_lens);
            *pp = zero + regs[c];
            break;
        }

        /* Addition */
        else if (opcode == 3)
        {
            regs[a] = (regs[b] + regs[c]) % POWER;
        }

        /* Conditional Move */
        else if (opcode == 0)
        {
            if (regs[c] != 0)
                regs[a] = regs[b];
        }

        /* Map Segment (Memory Module) */
        else if (opcode == 8)
            regs[b] = map_segment(regs[c], segs, seg_size,
                                  seg_cap, seg_lens, *rec_ids, rec_size);

        /* Unmap Segment (Memory Module) */
        else if (opcode == 9)
            unmap_segment(regs[c], rec_ids, rec_size, rec_cap);

        /* Division */
        else if (opcode == 5)
            regs[a] = regs[b] / regs[c];

        /* Multiplication */
        else if (opcode == 4)
        {
            regs[a] = (regs[b] * regs[c]) % POWER;
        }

        /* Output (IO) */
        else if (opcode == 10)
            putchar((unsigned char)regs[c]);

        /* Input (IO) NOTE: intentionally omitted*/
        // else if (opcode == 11) regs[c] = getc(stdin);

        /* Halt or Invalid Opcode*/
        else
            handle_halt(segs, seg_size, seg_lens, rec_ids);
    }
}

static inline void fill_cache(Instruction *cache, Instruction **program_pointer)
{
    for (unsigned i = 0; i < NUM_INSTRS; i++)
        cache[i] = (*program_pointer)[i];
    *program_pointer += NUM_INSTRS;
}

void handle_instructions(uint32_t *zero, uint32_t fsize)
{
    uint32_t seg_cap = 128;
    uint32_t seg_size = 0;
    uint32_t **segs = (uint32_t **)calloc(seg_cap, sizeof(uint32_t *));
    uint32_t *seg_lens = (uint32_t *)calloc(seg_cap, sizeof(uint32_t));

    uint32_t rec_cap = 128;
    uint32_t rec_size = 0;
    uint32_t *rec_ids = (uint32_t *)calloc(seg_cap, sizeof(uint32_t));

    segs[0] = zero;
    seg_lens[0] = fsize;
    seg_size++;

    uint32_t regs[NUM_REGISTERS] = {0};
    Instruction *pp = zero;
    Instruction cache[NUM_INSTRS];

    while (true)
    {
        fill_cache(cache, &pp);
        
        empty_cache(cache, &pp, regs, zero, &segs, &seg_lens, &seg_size, &seg_cap,
                    &rec_ids, &rec_size, &rec_cap);
    }
}

// TODO: Do a bit of cleanup down here
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./um [executable.um]\n");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r");

    if (fp == NULL)
    {
        fprintf(stderr, "File %s could not be opened.\n", argv[1]);
        return EXIT_FAILURE;
    }

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
        fsize = file_stat.st_size;
    uint32_t *zero_segment = initialize_memory(fp, fsize + (NUM_INSTRS * sizeof(Instruction)));
    handle_instructions(zero_segment, fsize);
    return EXIT_SUCCESS;
}