#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "um_utils.h"

// TODO: make assembly format consistent (capitalization, spacing, etc.)

typedef void* (*Function)(void);

struct GlobalState gs;

void *initialize_zero_segment(size_t fsize);
uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);
size_t zero_all_registers(void *zero, size_t offset);
void load_zero_segment(void *zero, FILE *fp, size_t fsize);


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

    // Initialize the global state
    gs.pc = 0;
    gs.segment_sequence = NULL;
    gs.seq_size = 0;
    gs.seq_capacity = 0;
    gs.segment_lengths = NULL;
    gs.recycled_ids = NULL;
    gs.rec_size = 0;
    gs.rec_capacity = 0;

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
        fsize = file_stat.st_size;

    printf("Starting program.\n");
    void *zero = initialize_zero_segment(fsize);

    load_zero_segment(zero, fp, fsize);

    void *curr_seg = zero;

    // TODO: don't forget about offset (as in program counter!)
    while (curr_seg != NULL) {
        Function func = (Function)curr_seg;
        curr_seg = func();
    }

    printf("\nFinished Program.\n");

    /* Free zero segment */
    assert(munmap(zero, fsize) != -1);

    // TODO: need to free all memory segments

    fclose(fp);
    return 0;
}

void *initialize_zero_segment(size_t fsize)
{
    void *zero = mmap(NULL, fsize, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zero != MAP_FAILED);

    memset(zero, 0, fsize);
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

void load_zero_segment(void *zero, FILE *fp, size_t fsize)
{
    (void)fsize;
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;
    size_t offset = 0;
    offset = zero_all_registers(zero, offset);

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

            /* At this point, the word is assembled and ready to be compiled
             * into assembly */
            offset = compile_instruction(zero, word, offset);
            word = 0;
        }
        i++;
    }
}

size_t zero_all_registers(void *zero, size_t offset)
{
    assert(offset == 0);

    unsigned char *p = zero + offset;

    // zeroing out all 8 registers

    // xor r8, r8 (sets it to 0)
    *p++ = 0x4D;
    *p++ = 0x31;
    *p++ = 0xc0;

    // mov r9, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc1;

    // mov r10, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc2;

    // mov r11, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc3;

    // mov r12, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc4;

    // mov r13, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc5;

    // mov r14, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc6;

    // mov r15, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc7;

    return 24;
}