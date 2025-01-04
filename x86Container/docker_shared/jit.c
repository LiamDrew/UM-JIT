#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "um_utils.h"

#define ICAP 128
typedef void* (*Function)(void);
struct GlobalState gs;

uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);
void *initialize_zero_segment(size_t fsize);
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

    /* Initializing the global state variables */

    // Setting the program counter to 0
    gs.pc = 0;

    /* Initializing the size and capacity of the memory segment array */
    gs.seq_size = 0;
    gs.seq_cap = ICAP;

    /* Sequence of executable memory segments */
    // gs.program_seq = calloc(gs.seq_cap, sizeof(void*));
    gs.active = NULL;

    /* Sequence of UM words segments (needed for loading and storing) */
    gs.val_seq = calloc(gs.seq_cap, sizeof(uint32_t*));

    /* Array of segment sizes */
    gs.seg_lens = calloc(gs.seq_cap, sizeof(uint32_t));

    /* Initializing the size and capacity of the recycled segments array */
    gs.rec_size = 0;
    gs.rec_cap = ICAP;

    /* Sequence of recycled segment IDs */
    gs.rec_ids = calloc(gs.rec_cap, sizeof(uint32_t));

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0) {
        fsize = file_stat.st_size;
        assert((fsize % 4) == 0);
    }

    /* Initialize executable and non-executable memory for the zero segment
     * fsize gives the space for UM words, multiply by 4 for machine 
     * instructions */
    void *zero = initialize_zero_segment(fsize * MULT);
    // printf("The address of the zero executable segment is %p\n", zero);
    uint32_t *zero_vals = calloc(fsize, sizeof(uint32_t));
    load_zero_segment(zero, zero_vals, fp, fsize);

    // gs.program_seq[0] = zero;
    gs.val_seq[0] = zero_vals;
    gs.seg_lens[0] = (fsize / 4);
    gs.seq_size++;

    gs.active = zero;
    void *curr_seg = zero;

    /* Zero out all registers r8-r15 for JIT use */
    asm volatile(
        "movq $0, %%r8\n\t"
        "movq $0, %%r9\n\t"
        "movq $0, %%r10\n\t"
        "movq $0, %%r11\n\t"
        "movq $0, %%r12\n\t"
        "movq $0, %%r13\n\t"
        "movq $0, %%r14\n\t"
        "movq $0, %%r15\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" // Clobber list
    );

    while (curr_seg != NULL) {
        Function func = (Function)(curr_seg + (gs.pc * CHUNK));
        curr_seg = func();
    }

    /* Free all program segments */
    for (uint32_t i = 0; i < gs.seq_size; i++) {
        // munmap(gs.program_seq[i], gs.seg_lens[i] * CHUNK);
        free(gs.val_seq[i]);
    }

    // free(gs.program_seq);
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

void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t fsize)
{
    (void)fsize;
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;
    size_t offset = 0;

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

// asm volatile(
//     "pushq %%rdi\n\t"
//     "pushq %%rsi\n\t"
//     "pushq %%rdx\n\t"
//     "pushq %%rcx\n\t"
//     "pushq %%r8\n\t"
//     "pushq %%r9\n\t"
//     "pushq %%rax\n\t"
//     "pushq %%r10\n\t"
//     "pushq %%r11\n\t"
//     :
//     :
//     : "memory");
// // print_registers();

// asm volatile(
//     "popq %%r11\n\t"
//     "popq %%r10\n\t"
//     "popq %%rax\n\t"
//     "popq %%r9\n\t"
//     "popq %%r8\n\t"
//     "popq %%rcx\n\t"
//     "popq %%rdx\n\t"
//     "popq %%rsi\n\t"
//     "popq %%rdi\n\t"
//     :
//     :
//     : "memory");