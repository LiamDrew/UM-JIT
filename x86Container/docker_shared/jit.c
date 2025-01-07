#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "um_utils.h"

typedef void *(*Function)(void);
struct GlobalState gs;

struct MachineCode mc;

uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);
void *initialize_zero_segment(size_t fsize);
size_t zero_all_registers(void *zero, size_t offset);
void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t fsize);
void *init_registers();
void initialize_instruction_bank();

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
    // mc.seg_bytes = {11, 31, 31, 11, 11, 14, 14, 4, 36, 33, 33, 17, 40, 0, 0};

    uint32_t init_values[] = {11, 31, 31, 11, 11, 14, 14, 4, 36, 33, 33, 17, 40, 0, 0};
    memcpy(mc.seg_bytes, init_values, sizeof(init_values));
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
    initialize_instruction_bank();
    // mc.bank = initialize_instruction_bank();

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

void initialize_instruction_bank()
{
    unsigned char *bank = mc.bank;
    uint32_t offset = 0;

    unsigned i = 0;

    // Conditional Move
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 8;

    mc.ou[i].bsi = offset + 8;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 2;
    i++;

    offset += cond_move(bank, offset, 0, 0, 0);

    // Segmented Load
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 28;

    mc.ou[i].bsi = offset + 15;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 18;
    mc.ou[i].ci = 0;
    i++;

    offset += inject_seg_load(bank, offset, 0, 0, 0);

    // Segmented Store
    mc.ou[i].asi = offset + 15;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = offset + 18;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 21;
    mc.ou[i].ci = 0;
    i++;

    offset += inject_seg_store(bank, offset, 0, 0, 0);

    // Addition
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 8;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 5;
    mc.ou[i].ci = 0;
    i++;

    offset += add_regs(bank, offset, 0, 0, 0);

    // Multiplication
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 8;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 5;
    i++;

    offset += mult_regs(bank, offset, 0, 0, 0);

    // Division
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 11;

    mc.ou[i].bsi = offset + 5;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 8;
    i++;

    offset += div_regs(bank, offset, 0, 0, 0);

    // Bitwise NAND
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 11;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 5;
    mc.ou[i].ci = 0;
    i++;

    offset += nand_regs(bank, offset, 0, 0, 0);

    // Halt
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;

    offset += handle_halt(bank, offset);

    // Map Segment
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = offset + 33;

    mc.ou[i].csi = offset + 2;
    mc.ou[i].ci = 0;
    i++;

    offset += inject_map_segment(bank, offset, 0, 0);

    // Unmap Segment
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    // mc.ou[i].csi = offset + 56; // Unrolled
    mc.ou[i].csi = offset + 2; // Rolled
    mc.ou[i].ci = 0;
    i++;

    offset += inject_unmap_segment(bank, offset, 0);

    // Output
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 2;
    mc.ou[i].ci = 0;
    i++;

    offset += print_reg(bank, offset, 0);

    // Input
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 14;
    i++;

    offset += read_into_reg(bank, offset, 0);

    // Load Program
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 5;
    mc.ou[i].ci = 0;
    i++;

    offset += inject_load_program(bank, offset, 0, 0);

    // Load Value
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;

    offset += CHUNK;

    // Opcode 14
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;
    offset += CHUNK;

    // Opcode 15
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;

    offset += CHUNK;
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