#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>

#define NUM_REGISTERS 8
#define POWER ((uint64_t)1 << 32);
#define NUM_INSTRS 1

typedef uint32_t Instruction;

// Array of segments
uint32_t **segment_sequence = NULL;
uint32_t seq_size = 0;
uint32_t seq_capacity = 0;

// Corresponding length of each segment
uint32_t *segment_lengths = NULL;

// Array of recycled IDs
uint32_t *recycled_ids = NULL;
uint32_t rec_size = 0;
uint32_t rec_capacity = 0;

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

/* DRIVERS/CONTROLLERS */

uint32_t *initialize_memory(FILE *fp, size_t fsize)
{
    /* MEMORY */

    seq_capacity = 128;
    segment_sequence = (uint32_t **)calloc(seq_capacity, sizeof(uint32_t *));
    segment_lengths = (uint32_t *)calloc(seq_capacity, sizeof(uint32_t));

    rec_capacity = 128;
    recycled_ids = (uint32_t *)calloc(rec_capacity, sizeof(uint32_t *));

    // load initial segment
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
    segment_sequence[0] = temp;
    segment_lengths[0] = fsize; // length is the number of words, not number of bytes
    seq_size++;

    return temp;
}

/* INSTRUCTION FETCHING */

void handle_halt()
{
    for (uint32_t i = 0; i < seq_size; i++)
        free(segment_sequence[i]);
    free(segment_sequence);
    free(segment_lengths);
    free(recycled_ids);
    exit(EXIT_SUCCESS);
}

uint32_t map_segment(uint32_t size)
{
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (rec_size == 0)
    {
        if (seq_size == seq_capacity)
        { // expand if necessary
            seq_capacity = seq_capacity * 2 + 2;
            segment_lengths = (uint32_t *)realloc(segment_lengths, (seq_capacity) * sizeof(uint32_t));
            segment_sequence = (uint32_t **)realloc(segment_sequence, (seq_capacity) * sizeof(uint32_t *));

            for (uint32_t i = seq_size; i < seq_capacity; i++)
            {
                segment_sequence[i] = NULL;
                segment_lengths[i] = 0;
            }
        }

        new_seg_id = seq_size++;
    }

    /* Otherwise, reuse an old one */
    else
        new_seg_id = recycled_ids[--rec_size];

    if (segment_sequence[new_seg_id] == NULL || size > segment_lengths[new_seg_id])
    {
        segment_sequence[new_seg_id] = (uint32_t *)realloc(segment_sequence[new_seg_id], size * sizeof(uint32_t));
        segment_lengths[new_seg_id] = size;
    }

    // Update the segment length and zero out memory
    // NOTE: we do not want to reset the size unless we have to
    memset(segment_sequence[new_seg_id], 0, size * sizeof(uint32_t));

    return new_seg_id;
}

void unmap_segment(uint32_t segment)
{
    if (rec_size == rec_capacity)
    {
        rec_capacity = rec_capacity * 2 + 2;
        recycled_ids = (uint32_t *)realloc(recycled_ids, (rec_capacity) * sizeof(uint32_t));
    }

    recycled_ids[rec_size++] = segment;
}

void load_segment(uint32_t index, uint32_t *zero)
{
    if (index > 0)
    {
        uint32_t copied_seq_size = segment_lengths[index];
        memcpy(zero, segment_sequence[index], copied_seq_size * sizeof(uint32_t));
    }
}

static inline Instruction get_instr(Instruction *cache, unsigned index)
{
    return cache[index];
}

// static inline uint32_t decode_instruction(uint32_t word, uint32_t *a, uint32_t *b,
//                                           uint32_t *c, uint32_t *value_to_load)
// {
//     uint32_t opcode = (word >> 28) & 0xF;

//     if (opcode == 13)
//     {
//         *a = (word >> 25) & 0x7;
//         *value_to_load = word & 0x1FFFFFF;
//     }
//     else
//     {
//         *c = word & 0x7;
//         *b = (word >> 3) & 0x7;
//         *a = (word >> 6) & 0x7;
//     }

//     return opcode;
// }

static inline void empty_cache(Instruction *cache, Instruction **pp, uint32_t *regs, uint32_t *zero)
{
    Instruction word;
    uint32_t a = 0, b = 0, c = 0, val = 0;

    for (unsigned i = 0; i < NUM_INSTRS; i++)
    {
        word = get_instr(cache, i);
        uint32_t opcode = (word >> 28) & 0xF;

        /* Load Value */
        if (opcode == 13)
        {
            a = (word >> 25) & 0x7;
            val = word & 0x1FFFFFF;
            regs[a] = val;
            return;
        }

        c = word & 0x7;
        b = (word >> 3) & 0x7;
        a = (word >> 6) & 0x7;

        switch (opcode)
        {

        /* Segmented Load (Memory Module) */
        case 1:
            regs[a] = segment_sequence[regs[b]][regs[c]];
            break;

        /* Segmented Store (Memory Module) */
        case 2:
            segment_sequence[regs[a]][regs[b]] = regs[c];
            break;

        /* Bitwise NAND */
        case 6:
            regs[a] = ~(regs[b] & regs[c]);
            break;

        /* Load Segment (Memory Module) */
        case 12:
            load_segment(regs[b], zero);
            *pp = zero + regs[c];
            break;

        /* Addition */
        case 3: // Addition
            regs[a] = (regs[b] + regs[c]) % POWER;
            break;

        /* Conditional Move */
        case 0:
            if (regs[c] != 0)
                regs[a] = regs[b];
            break;

        /* Map Segment (Memory Module) */
        case 8:
            regs[b] = map_segment(regs[c]);
            break;

        /* Unmap Segment (Memory Module) */
        case 9:
            unmap_segment(regs[c]);
            break;

        /* Division */
        case 5:
            regs[a] = regs[b] / regs[c];
            break;

        /* Multiplication */
        case 4:
            regs[a] = (regs[b] * regs[c]) % POWER;
            break;

        /* Output (IO) */
        case 10:
            putchar((unsigned char)regs[c]);
            break;

        /* Input (IO) NOTE: intentionally omitted*/
        // else if (opcode == 11) regs[c] = getc(stdin);

        /* Halt or Invalid Opcode*/
        default:
            handle_halt();
        }
    }
}

static inline void fill_cache(Instruction *cache, Instruction **program_pointer)
{
    for (unsigned i = 0; i < NUM_INSTRS; i++)
        cache[i] = (*program_pointer)[i];
    *program_pointer += NUM_INSTRS;
}

// #define DEBUG true
void handle_instructions(uint32_t *zero)
{
    // Initialize memory
    uint32_t regs[NUM_REGISTERS] = {0};
    Instruction *pp = zero;
    Instruction cache[NUM_INSTRS];

    while (true)
    {
#ifdef DEBUG
        asm volatile("marker_label: .word 0xDEADBEEF");
#endif

        // fill_cache(cache, &pp);
        cache[0] = *pp;
        pp += NUM_INSTRS;

#ifdef DEBUG
        asm volatile("marker_label2: .word 0xDEADBEEF");
#endif

        empty_cache(cache, &pp, regs, zero);
    }
}

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
    handle_instructions(zero_segment);
    return EXIT_SUCCESS;
}