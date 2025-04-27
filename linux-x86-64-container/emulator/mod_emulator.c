/**
 * @file mod_emulator.c
 * @author Liam Drew
 * @date March 2025
 * @brief
 * This UM implementation is modified to use the Virt32 memory system. In order
 * to compile with gcc and Virt32, I had to modify a few function definitions.
 * For now, these are experiemental. Any permanent changes will be made in the
 * Virt32 repository.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>

#include "virt.h"

#define NUM_REGISTERS 8
#define POWER ((uint64_t)1 << 32) /* prevent 32-bit overflow with add & div */
typedef uint32_t Instruction;

void initialize_memory(FILE *fp, size_t fsize, uint8_t *umem);
uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);

void handle_instructions(uint8_t *mem);
static inline bool exec_instr(Instruction word,
                              uint32_t *regs, uint8_t *umem,
                              uint32_t *pc);
static inline uint32_t map_segment(uint8_t *umem, uint32_t size);

static inline void unmap_segment(uint32_t segment);
inline void load_segment(uint32_t index, uint8_t *umem);

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

    uint8_t *umem = init_memory_system(KERN_SIZE);

    initialize_memory(fp, fsize + sizeof(Instruction), umem);

    handle_instructions(umem);

    terminate_memory_system();

    return EXIT_SUCCESS;
}

void initialize_memory(FILE *fp, size_t fsize, uint8_t *umem)
{
    /* Here, fsize is already adjusted to account for the fact that each
     * UM instruction is 4 bytes. Future allocations will have to take this into
     * account. */
    kern_realloc(fsize);
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;

    for (c = getc(fp); c != EOF; c = getc(fp))
    {
        c_char = (unsigned char)c;
        if (i % 4 == 0)
            word = assemble_word(word, 8, 24, c_char);
        else if (i % 4 == 1)
            word = assemble_word(word, 8, 16, c_char);
        else if (i % 4 == 2)
            word = assemble_word(word, 8, 8, c_char);
        else if (i % 4 == 3)
        {
            word = assemble_word(word, 8, 0, c_char);

            // storing in the zero segment here
            set_at(umem, 0 + (i / 4) * sizeof(uint32_t), word);
            word = 0;
        }

        i++;
    }

    fclose(fp);
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

void handle_instructions(uint8_t *umem)
{
    uint32_t regs[NUM_REGISTERS] = {0};
    uint32_t pc = 0;
    Instruction word;

    bool exit = false;

    while (!exit)
    {
        word = get_at(umem, 0 + pc * sizeof(uint32_t));
        pc++;
        exit = exec_instr(word, regs, umem, &pc);
    }
}

static inline bool exec_instr(Instruction word, uint32_t *regs, uint8_t *umem,
                              uint32_t *pc)
{
    uint32_t a = 0, b = 0, c = 0, val = 0;
    uint32_t opcode = word >> 28;

    /* Load Value */
    if (__builtin_expect(opcode == 13, 1))
    {
        a = (word >> 25) & 0x7;
        val = word & 0x1FFFFFF;
        regs[a] = val;
        return false;
    }

    c = word & 0x7;
    b = (word >> 3) & 0x7;
    a = (word >> 6) & 0x7;

    /* Segmented Load */
    if (__builtin_expect(opcode == 1, 1))
    {
        regs[a] = get_at(umem, regs[b] + regs[c] * sizeof(uint32_t));
    }

    /* Segmented Store */
    else if (__builtin_expect(opcode == 2, 1))
    {
        set_at(umem, regs[a] + regs[b] * sizeof(uint32_t), regs[c]);
    }

    /* Bitwise NAND */
    else if (__builtin_expect(opcode == 6, 1))
    {
        regs[a] = ~(regs[b] & regs[c]);
    }

    /* Load Segment */
    else if (__builtin_expect(opcode == 12, 0))
    {
        load_segment(regs[b], umem);
        *pc = regs[c]; // intentionally not multiplying
    }

    /* Addition */
    else if (__builtin_expect(opcode == 3, 0))
    {
        regs[a] = (regs[b] + regs[c]) % POWER;
    }

    /* Conditional Move */
    else if (__builtin_expect(opcode == 0, 0))
    {
        if (regs[c] != 0)
            regs[a] = regs[b];
    }

    /* Map Segment */
    else if (__builtin_expect(opcode == 8, 0))
    {
        regs[b] = map_segment(umem, regs[c]);
    }

    /* Unmap Segment */
    else if (__builtin_expect(opcode == 9, 0))
    {
        unmap_segment(regs[c]);
    }

    /* Division */
    else if (__builtin_expect(opcode == 5, 0))
    {
        regs[a] = regs[b] / regs[c];
    }

    /* Multiplication */
    else if (__builtin_expect(opcode == 4, 0))
    {
        regs[a] = (regs[b] * regs[c]) % POWER;
    }

    /* Output */
    else if (__builtin_expect(opcode == 10, 0))
    {
        putchar((unsigned char)regs[c]);
    }

    /* Input */
    else if (__builtin_expect(opcode == 11, 0))
    {
        regs[c] = getc(stdin);
    }

    /* Stop or Invalid Instruction */
    else
    {
        return true;
    }

    return false;
}

static inline uint32_t map_segment(uint8_t *umem, uint32_t size)
{
    return vs_calloc(umem, size * sizeof(uint32_t));
}

static inline void unmap_segment(uint32_t segment)
{
    vs_free(segment);
}

inline void load_segment(uint32_t index, uint8_t *umem)
{
    /* Return immediately if loading elsewhere in the zero segment */
    if (index == 0)
        return;

    /* Get the size of the segment we want to duplicate */
    uint32_t *seg_addr = (uint32_t *)convert_address(umem, index);
    uint32_t copy_size = seg_addr[-1];

    /* Reallocate the kernel size and copy the new segment into it */
    kern_realloc(copy_size);
    kern_memcpy(index, copy_size);
}
