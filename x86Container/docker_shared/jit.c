#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "um_utils.h"

#define ICAP 128
typedef void *(*Function)(void);
struct GlobalState gs;

struct MachineCode mc;

uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);
void *initialize_zero_segment(size_t fsize);
size_t zero_all_registers(void *zero, size_t offset);
void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t fsize);
void *init_registers();
void *initialize_instruction_bank();

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

    // gs.handle_realloc_ptr = (void *)&handle_realloc;

    // Setting the program counter to 0
    gs.pc = 0;

    /* Initializing the size and capacity of the memory segment array */
    gs.seq_size = 0;
    gs.seq_cap = ICAP;

    /* Sequence of executable memory segments */
    // gs.program_seq = calloc(gs.seq_cap, sizeof(void*));
    gs.active = NULL;

    /* Sequence of UM words segments (needed for loading and storing) */
    gs.val_seq = calloc(gs.seq_cap, sizeof(uint32_t *));

    /* Array of segment sizes */
    gs.seg_lens = calloc(gs.seq_cap, sizeof(uint32_t));

    /* Initializing the size and capacity of the recycled segments array */
    gs.rec_size = 0;
    gs.rec_cap = ICAP;

    /* Sequence of recycled segment IDs */
    gs.rec_ids = calloc(gs.rec_cap, sizeof(uint32_t));

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
    {
        fsize = file_stat.st_size;
        assert((fsize % 4) == 0);
    }

    /* This function hardcodes the addresses of a, b, and c that need to get 
     * injected. They are stored in the MachineCode struct */
    mc.bank = initialize_instruction_bank();

    /* Initialize executable and non-executable memory for the zero segment */
    void *zero = initialize_zero_segment(fsize * MULT);
    uint32_t *zero_vals = calloc(fsize, sizeof(uint32_t));
    assert(zero_vals != NULL);


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

    while (curr_seg != NULL)
    {
        Function func = (Function)(curr_seg + (gs.pc * CHUNK));
        curr_seg = func();
    }

    /* Free all program segments */
    for (uint32_t i = 0; i < gs.seq_size; i++)
    {
        free(gs.val_seq[i]);
    }

    free(gs.val_seq);
    free(gs.seg_lens);
    free(gs.rec_ids);

    fclose(fp);
    return 0;
}

void *initialize_zero_segment(size_t asmbytes)
{
    void *hint_addr = (void *)((uintptr_t)&gs & ~0xFFF); // Round down to page boundary
    void *zero = mmap(hint_addr, asmbytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zero != MAP_FAILED);

    memset(zero, 0, asmbytes);
    return zero;
}

void *initialize_instruction_bank()
{
    // printf("instruction bank being initialized\n");
    void *bank = mmap(NULL, CHUNK * OPS, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(bank != MAP_FAILED);
    memset(bank, 0, CHUNK * OPS);

    uint32_t offset = 0;

    // Need to make an array of a indexes, b indexes, and c indexes
    // at each index, an instruction needs to be modified

    // also need to keep track of how many bytes they need to be shifted in.

    uint8_t asi = 0;
    uint8_t ai = 0;

    uint8_t bsi = 0;
    uint8_t bi = 0;

    uint8_t csi = 0;
    uint8_t ci = 0;

    // Conditional Move
    mc.c[ci++] = offset + 2; // c at index 2 (no shift)
    offset += cond_move(bank, offset, 0, 0, 0);
    
    // Segmented Load
    mc.b_shift[bsi++] = offset + 15; // b at index 15 (shift << 3)
    mc.c_shift[csi++] = offset + 18; // c at index 18 (shift << 3)
    mc.a[ai++] = offset + 28;        // a at index 28 (no shift)

    offset += inject_seg_load(bank, offset, 0, 0, 0);

    // Segmented Store
    mc.a_shift[asi++] = offset + 15; // a at index 15 (shift << 3)
    mc.b_shift[bsi++] = offset + 18; // b at index 18 (shift << 3)
    mc.c_shift[csi++] = offset + 21; // c at index 21 (shift << 3)

    offset += inject_seg_store(bank, offset, 0, 0, 0);

    // Addition
    mc.b_shift[bsi++] = offset + 2; // b at index 2 (shift << 3)
    mc.c_shift[csi++] = offset + 5; // c at index 5 (shift << 3)
    mc.a[ai++] = offset + 8;        // a at index 8 (no shift)
    offset += add_regs(bank, offset, 0, 0, 0);

    // Multiplication
    mc.b_shift[bsi++] = offset + 2; // b at index 2 (shift << 3)
    mc.c[ci++] = offset + 5;        // c at index 5 (no shift)
    mc.a[ai++] = offset + 8;        // a at index 8 (no shift)
    offset += mult_regs(bank, offset, 0, 0, 0);

    // Division
    mc.b_shift[bsi++] = offset + 5; // b at index 5 (shift << 3)
    mc.c[ci++] = offset + 8;        // c at index 8 (no shift)
    mc.a[ai++] = offset + 11;       // a at index 11 (no shift)
    offset += div_regs(bank, offset, 0, 0, 0);

    // Bitwise NAND
    mc.b_shift[bsi++] = offset + 2; // b at index 2 (shift << 3)
    mc.c_shift[csi++] = offset + 5; // c at index 5 (shift << 3)
    mc.a[ai++] = offset + 11;       // a at index 11 (no shift)
    offset += nand_regs(bank, offset, 0, 0, 0);

    // Halt
    // No regs
    offset += handle_halt(bank, offset);

    // Map Segment
    mc.c_shift[csi++] = offset + 2; // c at index 2 (shift << 3)
    mc.b[bi++] = offset + 33;       // b at index 33 (no shift)
    offset += inject_map_segment(bank, offset, 0, 0);

    // Unmap Segment
    // NOTE: (For the unrolled version)
    mc.c_shift[csi++] = offset + 56; // c at index 56 (shift << 3)
    offset += inject_unmap_segment(bank, offset, 0);

    // Output
    mc.c_shift[csi++] = offset + 2; // c at index 2 (shift << 3)
    offset += print_reg(bank, offset, 0);

    // Input
    mc.c[ci++] = offset + 14; // c at index 14 (no shift)
    offset += read_into_reg(bank, offset, 0);

    // Load Program
    mc.b_shift[bsi++] = offset + 2; // b at index 2 (shift << 3)
    mc.c_shift[csi++] = offset + 5; // c at index 5 (shift << 3)
    offset += inject_load_program(bank, offset, 0, 0);
    
    // Load Value
    // This case gets handled at the beginning due to the weirdness when loading value
    offset += CHUNK;

    // Opcode 14
    offset += CHUNK;

    // Opcode 15
    offset += CHUNK;

    assert(asi == AS);
    assert(ai == A);

    assert(bsi == BS);
    assert(bi == B);

    assert(csi == CS);
    assert(ci == C);

    return bank;
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