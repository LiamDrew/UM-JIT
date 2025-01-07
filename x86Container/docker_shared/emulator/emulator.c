/**
 * @file main.c
 * @author Liam Drew
 * @date 2024-12-27
 * @brief
 * 
 * This program implements a virtual machine. The VM recognizes 14 instructions
 * and only has 8 registers, but has boundless 4 byte oriented memory
 * that is only limited by the memory of the host machine.
 *
 * This VM has been profiled for an x86 linux system hosted in a Docker
 * container on an Apple silicon machine. It runs the benchmark assembly
 * language program in 2.8 seconds
 *
 * Other minor variations of this program have been profiled on ARM
 * systems. Natively on MacOS, the benchmark runs in 2.5 seconds. Remarkably,
 * on an Aarch64 linux system hosted on the same Apple silicon machine, the
 * benchmark runs in 2.3 seconds.
 *
 * I intend for this VM to be used as a starting benchmark for the
 * VM assembly to x86 assembly just-in-time compiler I'm building.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>

#define NUM_REGISTERS 8
#define POWER ((uint64_t)1 << 32)   // for preventing overflow with add and div
#define ICAP 32500                  // determined experimentally for sandmark

typedef uint32_t Instruction;

/* Sequence of program segments */
uint32_t **segment_sequence = NULL;
uint32_t seq_size = 0;
uint32_t seq_capacity = 0;
uint32_t *segment_lengths = NULL;

/* Sequence of recycled segments */
uint32_t *recycled_ids = NULL;
uint32_t rec_size = 0;
uint32_t rec_capacity = 0;

uint32_t *initialize_memory(FILE *fp, size_t fsize);
uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);

void handle_instructions(uint32_t *zero);
void handle_stop();
static inline bool exec_instr(Instruction word, Instruction **pp,
                               uint32_t *regs, uint32_t *zero);
uint32_t map_segment(uint32_t size);
void unmap_segment(uint32_t segment);
void load_segment(uint32_t index, uint32_t *zero);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: ./um [executable.um]\n");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "File %s could not be opened.\n", argv[1]);
        return EXIT_FAILURE;
    }

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
        fsize = file_stat.st_size;

    uint32_t *zero_segment = initialize_memory(fp, fsize + sizeof(Instruction));
    handle_instructions(zero_segment);
    return EXIT_SUCCESS;
}

uint32_t *initialize_memory(FILE *fp, size_t fsize)
{
    seq_capacity = ICAP;
    segment_sequence = (uint32_t **)calloc(seq_capacity, sizeof(uint32_t *));
    segment_lengths = (uint32_t *)calloc(seq_capacity, sizeof(uint32_t));

    rec_capacity = ICAP;
    recycled_ids = (uint32_t *)calloc(rec_capacity, sizeof(uint32_t *));

    /* Load initial segment from file */
    uint32_t *zero = (uint32_t *)calloc(fsize, sizeof(uint32_t));
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;

    for (c = getc(fp); c != EOF; c = getc(fp)) {
        c_char = (unsigned char)c;
        if (i % 4 == 0)
            word = assemble_word(word, 8, 24, c_char);
        else if (i % 4 == 1)
            word = assemble_word(word, 8, 16, c_char);
        else if (i % 4 == 2)
            word = assemble_word(word, 8, 8, c_char);
        else if (i % 4 == 3) {
            word = assemble_word(word, 8, 0, c_char);
            zero[i / 4] = word;
            word = 0;
        }
        i++;
    }

    fclose(fp);
    segment_sequence[0] = zero;
    segment_lengths[0] = fsize;
    seq_size++;

    return zero;
}

uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value)
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

void handle_instructions(uint32_t *zero)
{
    uint32_t regs[NUM_REGISTERS] = {0};
    Instruction *pp = zero;
    Instruction word;

    bool exit = false;

    while (!exit)
    {
        word = *pp;
        pp++;
        exit = exec_instr(word, &pp, regs, zero);
    }

    handle_stop();
}

static inline bool exec_instr(Instruction word, Instruction **pp, 
                               uint32_t *regs, uint32_t *zero)
{
    (void)zero;
    uint32_t a = 0, b = 0, c = 0, val = 0;
    uint32_t opcode = word >> 28;

    /* Load Value */
    if (__builtin_expect(opcode == 13, 1)) {
        a = (word >> 25) & 0x7;
        val = word & 0x1FFFFFF;
        regs[a] = val;
        return false;
    }

    c = word & 0x7;
    b = (word >> 3) & 0x7;
    a = (word >> 6) & 0x7;

    /* Segmented Load */
    if (__builtin_expect(opcode == 1, 1)) {
        regs[a] = segment_sequence[regs[b]][regs[c]];
    }

    /* Segmented Store */
    else if (__builtin_expect(opcode == 2, 1)) {
        segment_sequence[regs[a]][regs[b]] = regs[c];
    }

    /* Bitwise NAND */
    else if (__builtin_expect(opcode == 6, 1)) {
        regs[a] = ~(regs[b] & regs[c]);
    }

    /* Load Segment */
    else if (__builtin_expect(opcode == 12, 0))
    {
        load_segment(regs[b], zero);
        *pp = segment_sequence[0] + regs[c];
    }

    /* Addition */
    else if (__builtin_expect(opcode == 3, 0)) {
        regs[a] = (regs[b] + regs[c]) % POWER;
    }

    /* Conditional Move */
    else if (__builtin_expect(opcode == 0, 0))
    {
        if (regs[c] != 0)
            regs[a] = regs[b];
    }

    /* Map Segment */
    else if (__builtin_expect(opcode == 8, 0)) {
        regs[b] = map_segment(regs[c]);
    }

    /* Unmap Segment */
    else if (__builtin_expect(opcode == 9, 0)) {
        unmap_segment(regs[c]);
    }

    /* Division */
    else if (__builtin_expect(opcode == 5, 0)) {
        regs[a] = regs[b] / regs[c];
    }

    /* Multiplication */
    else if (__builtin_expect(opcode == 4, 0)) {
        regs[a] = (regs[b] * regs[c]) % POWER;
    }

    /* Output */
    else if (__builtin_expect(opcode == 10, 0)) {
        putchar((unsigned char)regs[c]);
    }

    /* Input */
    else if (__builtin_expect(opcode == 11, 0)) {
        regs[c] = getc(stdin);
    }

    /* Stop or Invalid Instruction */
    else {
        return true;
    }

    return false;
}

void handle_stop()
{
    for (uint32_t i = 0; i < seq_size; i++)
        free(segment_sequence[i]);
    free(segment_sequence);
    free(segment_lengths);
    free(recycled_ids);
}

uint32_t map_segment(uint32_t size)
{
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (rec_size == 0)
    {
        if (seq_size == seq_capacity)
        {
            /* Expand the sequence if necessary */
            seq_capacity = seq_capacity * 2 + 2;
            segment_lengths = (uint32_t *)realloc(segment_lengths,
                                                  (seq_capacity) * sizeof(uint32_t));
            segment_sequence = (uint32_t **)realloc(segment_sequence,
                                                    (seq_capacity) * sizeof(uint32_t *));

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

    if (segment_sequence[new_seg_id] == NULL ||
        size > segment_lengths[new_seg_id])
    {
        segment_sequence[new_seg_id] =
            (uint32_t *)realloc(segment_sequence[new_seg_id],
                                size * sizeof(uint32_t));
        segment_lengths[new_seg_id] = size;
    }

    /* Zero out the segment */
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
    (void)zero;
    if (index > 0)
    {
        uint32_t copied_seq_size = segment_lengths[index];

        uint32_t *new_zero = malloc(copied_seq_size * sizeof(uint32_t));
        memcpy(new_zero, segment_sequence[index], 
               copied_seq_size * sizeof(uint32_t));
        segment_sequence[0] = new_zero;
    }
}