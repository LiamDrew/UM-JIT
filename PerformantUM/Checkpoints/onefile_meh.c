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

static inline uint32_t decode_instruction(uint32_t word, uint32_t *a, uint32_t *b,
                                          uint32_t *c, uint32_t *value_to_load)
{
    uint32_t opcode = (word >> 28) & 0xF;

    if (opcode == 13)
    {
        *a = (word >> 25) & 0x7;
        *value_to_load = word & 0x1FFFFFF;
    }
    else
    {
        *c = word & 0x7;
        *b = (word >> 3) & 0x7;
        *a = (word >> 6) & 0x7;
    }

    return opcode;
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

    uint32_t seq_size = 0;
    uint32_t seq_capacity = 128;
    uint32_t **segment_sequence = (uint32_t **)calloc(seq_capacity, sizeof(uint32_t *));

    uint32_t *segment_lengths = (uint32_t *)calloc(seq_capacity, sizeof(uint32_t));

    uint32_t rec_size = 0;
    uint32_t rec_capacity = 128;
    uint32_t *recycled_ids = (uint32_t *)calloc(rec_capacity, sizeof(uint32_t *));

    // load initial segment
    uint32_t *zero = (uint32_t *)calloc(fsize, sizeof(uint32_t));
    uint32_t iword = 0;
    int ch;
    int i = 0;
    unsigned char c_char;

    for (ch = getc(fp); ch != EOF; ch = getc(fp))
    {
        c_char = (unsigned char)ch;
        if (i % 4 == 0)
            iword = Bitpack_newu(iword, 8, 24, c_char);
        else if (i % 4 == 1)
            iword = Bitpack_newu(iword, 8, 16, c_char);
        else if (i % 4 == 2)
            iword = Bitpack_newu(iword, 8, 8, c_char);
        else if (i % 4 == 3)
        {
            iword = Bitpack_newu(iword, 8, 0, c_char);
            zero[i / 4] = iword;
            iword = 0;
        }
        i++;
    }

    fclose(fp);
    segment_sequence[0] = zero;
    segment_lengths[0] = fsize;
    seq_size++;

    uint32_t regs[NUM_REGISTERS] = {0};
    Instruction *pp = zero;
    Instruction cache[NUM_INSTRS];

    Instruction word;
    uint32_t a = 0, b = 0, c = 0, val = 0;

    while (true)
    {
        for (unsigned i = 0; i < NUM_INSTRS; i++)
            cache[i] = pp[i];
        pp += NUM_INSTRS;

        for (unsigned i = 0; i < NUM_INSTRS; i++)
        {
            word = cache[i];
            uint32_t opcode = decode_instruction(word, &a, &b, &c, &val);

            /* Load Value */
            if (opcode == 13)
                regs[a] = val;

            /* Segmented Load (Memory Module) */
            else if (opcode == 1)
                regs[a] = segment_sequence[regs[b]][regs[c]];

            /* Segmented Store (Memory Module) */
            else if (opcode == 2)
                segment_sequence[regs[a]][regs[b]] = regs[c];

            /* Bitwise NAND */
            else if (opcode == 6)
                regs[a] = ~(regs[b] & regs[c]);

            /* Load Segment (Memory Module) */
            else if (opcode == 12)
            {
                uint32_t index = regs[b];
                if (index > 0)
                {
                    uint32_t copied_seq_size = segment_lengths[index];
                    memcpy(zero, segment_sequence[index], copied_seq_size * sizeof(uint32_t));
                }
                pp = zero + regs[c];
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
            {
                uint32_t size = regs[c];
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

                regs[b] = new_seg_id;
            }

            /* Unmap Segment (Memory Module) */
            else if (opcode == 9)
            {
                uint32_t segment = regs[c];
                if (rec_size == rec_capacity)
                {
                    rec_capacity = rec_capacity * 2 + 2;
                    recycled_ids = (uint32_t *)realloc(recycled_ids, (rec_capacity) * sizeof(uint32_t));
                }

                recycled_ids[rec_size++] = segment;
            }

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
            {
                for (uint32_t i = 0; i < seq_size; i++)
                    free(segment_sequence[i]);
                free(segment_sequence);
                free(segment_lengths);
                free(recycled_ids);
                return EXIT_SUCCESS;
            }
        }
    }

    return EXIT_SUCCESS;
}