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
void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t fsize);

void *init_registers();

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
    // 128 chosen arbitrarily
    gs.pc = 0;
    gs.seq_size = 0;
    gs.seq_cap = 128;

    // array of pointers to executable memory
    gs.program_seq = calloc(gs.seq_cap, sizeof(void*));
    gs.val_seq = calloc(gs.seq_cap, sizeof(uint32_t*));
    gs.seg_lens = calloc(gs.seq_cap, sizeof(uint32_t));

    gs.rec_size = 0;
    gs.rec_cap = 128;
    gs.rec_ids = calloc(gs.rec_cap, sizeof(uint32_t));

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
        fsize = file_stat.st_size;

    printf("Starting program.\n");

    void *init = init_registers();
    size_t x1 = zero_all_registers(init, 0);
    (void)x1;

    assert((fsize % 4) == 0);

    // Need to multiply the fsize by the byte multiplier to make space for instructions
    void *zero = initialize_zero_segment(fsize * MULT);


    uint32_t *zero_vals = calloc(fsize, sizeof(uint32_t));

    load_zero_segment(zero, zero_vals, fp, fsize);

    gs.program_seq[0] = zero;
    gs.val_seq[0] = zero_vals;

    void *curr_seg = zero;

    gs.seq_size++;

    // printf("Before zeroing registers\n");
    // print_registers();

    /* This was possibly the most frustrating bug I ever encountered. Apparently,
     * the standard library functions */

    register long r8 asm("r8");
    register long r9 asm("r9");
    register long r10 asm("r10");
    register long r11 asm("r11");
    register long r12 asm("r12");
    register long r13 asm("r13");
    register long r14 asm("r14");
    register long r15 asm("r15");

    (void)r8;
    (void)r9;
    (void)r10;
    (void)r11;
    (void)r12;
    (void)r13;
    (void)r14;
    (void)r15;

    // zero all the registers
    Function init_regs = (Function)init;
    void *garbage = init_regs();
    // printf("After zeroing registers\n");
    // print_registers();
    (void)garbage;
    (void)curr_seg;


    // print_registers();
    while (curr_seg != NULL) {
        // printf("counter is: %u\n", gs.pc);
        Function func = (Function)(curr_seg + (gs.pc * CHUNK));
        curr_seg = func();
        // print_registers();
        // assert(curr_seg == NULL);
    }

    printf("\nFinished Program.\n");

    // TODO: double check the way memory is freed
    /* Free zero segment (has the extra register zeroing instructions)*/
    munmap(init, 2 * CHUNK);
    assert(munmap(zero, fsize) != -1);

    /* Free all other memory */
    for (uint32_t i = 1; i < gs.seq_size; i++) {
        munmap(gs.program_seq[i], gs.seg_lens[i]); // TODO: double check this
        free(gs.val_seq[i]);
    }

    free(gs.program_seq);
    free(gs.val_seq);
    free(gs.seg_lens);
    free(gs.rec_ids);

    fclose(fp);
    return 0;
}

void *initialize_zero_segment(size_t asmbytes)
{
    void *zero = mmap(NULL, asmbytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zero != MAP_FAILED);

    memset(zero, 0, asmbytes);
    return zero;
}

void *init_registers()
{
    // It will take 24 bytes to write the instructions that zero all registers
    // we'll keep it a 32 byte boundary to stay fast
    void *start = mmap(NULL, 2 * CHUNK, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(start != MAP_FAILED);

    memset(start, 0, 2 * CHUNK);
    return start;
}

// void print_registers()
// {
//     uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

//     asm volatile(
//         "movq %%r8, %0\n\t"
//         "movq %%r9, %1\n\t"
//         "movq %%r10, %2\n\t"
//         "movq %%r11, %3\n\t"
//         "movq %%r12, %4\n\t"
//         "movq %%r13, %5\n\t"
//         "movq %%r14, %6\n\t"
//         "movq %%r15, %7\n\t"
//         : "=m"(r8), "=m"(r9), "=m"(r10), "=m"(r11),
//           "=m"(r12), "=m"(r13), "=m"(r14), "=m"(r15)
//         :
//         :);

//     printf("R8: %lu\nR9: %lu\nR10: %lu\nR11: %lu\n"
//            "R12: %lu\nR13: %lu\nR14: %lu\nR15: %lu\n",
//            r8, r9, r10, r11, r12, r13, r14, r15);
// }

size_t zero_all_registers(void *zero, size_t offset)
{
    assert(offset == 0);

    unsigned char *p = zero + offset;

    // zeroing out all 8 registers

    // xor r8, r8 (sets it to 0)
    // *p++ = 0x4D;
    // *p++ = 0x31;
    // *p++ = 0xc0;
    *p++ = 0x45; // REX.R prefix (no REX.W needed for 32-bit op)
    *p++ = 0x31; // XOR opcode
    *p++ = 0xc0; // ModR/M byte

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



    // 7 NoOPs to align with a 32 byte boundary
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;
    *p++ = 0xc3;

    return CHUNK * 2;
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

void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t fsize)
{
    (void)fsize;
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;
    size_t offset = 0;
    // offset = zero_all_registers(zero, offset);

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
            zero_vals[i / 4] = word;

            /* At this point, the word is assembled and ready to be compiled
             * into assembly */
            offset = compile_instruction(zero, word, offset);
            word = 0;
        }
        i++;
    }
}