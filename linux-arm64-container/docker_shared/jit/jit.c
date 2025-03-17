/**
 * @file jit.c
 * @author Liam Drew
 * @date March 2025
 * @brief
 * A Just-In-Time compiler from Universal Machine assembly language to
 * Arm assembly language.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include <sys/mman.h>

#include <arpa/inet.h>

#define OPS 15
#define INIT_CAP 32500

typedef uint32_t Instruction;

typedef void *(*Function)(void);

struct GlobalState
{
    uint32_t pc;
    void *active;
    uint32_t **val_seq;
    uint32_t *seg_lens;
    uint32_t seq_size;
    uint32_t seq_cap;

    uint32_t *rec_ids;
    uint32_t rec_size;
    uint32_t rec_cap;
} __attribute__((packed));

struct GlobalState gs;

void *initialize_zero_segment(size_t fsize);
void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t fsize);
uint64_t make_word(uint64_t word, unsigned width, unsigned lsb, uint64_t value);

size_t compile_instruction(void *zero, uint32_t word, size_t offset);
size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value);
size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t handle_halt(void *zero, size_t offset);
uint32_t map_segment(uint32_t size);
size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c);

void unmap_segment(uint32_t segmentID);
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c);

void print_out(uint32_t x);
size_t print_reg(void *zero, size_t offset, unsigned c);

unsigned char read_char(void);
size_t read_into_reg(void *zero, size_t offset, unsigned c);

void *load_program(uint32_t b_val);
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./jit [executable.um]\n");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r");

    if (fp == NULL)
    {
        fprintf(stderr, "File %s could not be opened.\n", argv[1]);
        return EXIT_FAILURE;
    }

    // Setting the program counter to 0
    gs.pc = 0;

    // Initializing the memory segment array
    gs.seq_size = 0;
    gs.seq_cap = INIT_CAP;
    gs.val_seq = calloc(gs.seq_cap, sizeof(uint32_t *));

    // Array of segment sizes
    gs.seg_lens = calloc(gs.seq_cap, sizeof(uint32_t));

    // Initializing the recycled segments array
    gs.rec_size = 0;
    gs.rec_cap = INIT_CAP;
    gs.rec_ids = calloc(gs.rec_cap, sizeof(uint32_t));

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
    {
        fsize = file_stat.st_size;
        assert((fsize % 4) == 0);
    }

    // Initialize executable and non-executable memory for the zero segment
    void *zero = initialize_zero_segment(fsize * ((CHUNK + 3) / 4));
    uint32_t *zero_vals = calloc(fsize, sizeof(uint32_t));
    load_zero_segment(zero, zero_vals, fp, fsize);
    fclose(fp);

    gs.val_seq[0] = zero_vals;
    gs.seg_lens[0] = (fsize / 4);
    gs.seq_size++;
    gs.active = zero;

    uint8_t *curr_seg = (uint8_t *)zero;

    run(curr_seg, gs.val_seq);

    // Free all program segments
    for (uint32_t i = 0; i < gs.seq_size; i++)
    {
        free(gs.val_seq[i]);
    }

    free(gs.val_seq);
    free(gs.seg_lens);
    free(gs.rec_ids);
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
            word = make_word(word, 8, 24, c_char);
        else if (i % 4 == 1)
            word = make_word(word, 8, 16, c_char);
        else if (i % 4 == 2)
            word = make_word(word, 8, 8, c_char);
        else if (i % 4 == 3)
        {
            word = make_word(word, 8, 0, c_char);
            zero_vals[i / 4] = word;

            // compile the UM word into machine code
            offset = compile_instruction(zero, word, offset);
            word = 0;
        }
        i++;
    }
}

uint64_t make_word(uint64_t word, unsigned width, unsigned lsb,
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

size_t compile_instruction(void *zero, Instruction word, size_t offset)
{
    uint32_t opcode = (word >> 28) & 0xF;
    // printf("Opcode is %u\n", opcode);
    uint32_t a = 0;

    // Load Value
    if (opcode == 13)
    {
        a = (word >> 25) & 0x7;
        uint32_t val = word & 0x1FFFFFF;
        offset += load_reg(zero, offset, a, val);
        return offset;
    }

    uint32_t b = 0, c = 0;

    c = word & 0x7;
    b = (word >> 3) & 0x7;
    a = (word >> 6) & 0x7;

    // Output
    if (opcode == 10)
    {
        offset += print_reg(zero, offset, c);
    }

    // Addition
    else if (opcode == 3)
    {
        offset += add_regs(zero, offset, a, b, c);
    }

    // Halt
    else if (opcode == 7)
    {
        offset += handle_halt(zero, offset);
    }

    // Bitwise NAND
    else if (opcode == 6)
    {
        offset += nand_regs(zero, offset, a, b, c);
    }

    // Addition
    else if (opcode == 3)
    {
        offset += add_regs(zero, offset, a, b, c);
    }

    // Multiplication
    else if (opcode == 4)
    {
        offset += mult_regs(zero, offset, a, b, c);
    }

    // Division
    else if (opcode == 5)
    {
        offset += div_regs(zero, offset, a, b, c);
    }

    // Conditional Move
    else if (opcode == 0)
    {
        offset += cond_move(zero, offset, a, b, c);
    }

    // Input
    else if (opcode == 11)
    {
        offset += read_into_reg(zero, offset, c);
    }

    // Segmented Load
    else if (opcode == 1)
    {
        offset += seg_load(zero, offset, a, b, c);
    }

    // Segmented Store
    else if (opcode == 2)
    {
        offset += seg_store(zero, offset, a, b, c);
    }

    // Load Program
    else if (opcode == 12)
    {
        offset += inject_load_program(zero, offset, b, c);
    }

    // Map Segment
    else if (opcode == 8)
    {
        offset += inject_map_segment(zero, offset, b, c);
    }

    // Unmap Segment
    else if (opcode == 9)
    {
        offset += inject_unmap_segment(zero, offset, c);
    }

    // Invalid Opcode
    else
    {
        offset += CHUNK;
    }

    return offset;
}

size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov w19, 0x0000
    uint32_t lower_mov = 0x52800000;    // base opcode for lower 16 bit MOV
    lower_mov |= (value & 0xFFFF) << 5; // Position lower 16 bits
    lower_mov |= BR + a; // (Update this to be 19 + reg number)

    *p++ = lower_mov & 0xFF;
    *p++ = (lower_mov >> 8) & 0xFF;
    *p++ = (lower_mov >> 16) & 0xFF;
    *p++ = (lower_mov >> 24) & 0xFF;

    // movk w19, #0x0000, lsl 16
    uint32_t upper_mov = 0x72A00000;
    upper_mov |= ((value >> 16) & 0xFFFF) << 5;
    upper_mov |= BR + a; // update this to be 19 + reg number

    *p++ = upper_mov & 0xFF;
    *p++ = (upper_mov >> 8) & 0xFF;
    *p++ = (upper_mov >> 16) & 0xFF;
    *p++ = (upper_mov >> 24) & 0xFF;

    // 3 No Ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // cmp w0, #0x0
    uint32_t cmp_instr = 0x7100001F;
    cmp_instr |= ((BR + c) << 5);
    *p++ = cmp_instr & 0xFF;
    *p++ = (cmp_instr >> 8) & 0xFF;
    *p++ = (cmp_instr >> 16) & 0xFF;
    *p++ = (cmp_instr >> 24) & 0xFF;

    // Then use CSEL with the NE (not equal) condition
    uint32_t csel_instr = 0x1A800000; // Base encoding for CSEL
    csel_instr |= ((BR + a));         // Destination register (rA)
    csel_instr |= ((BR + b) << 5);    // First source register (rB) - selected if condition is true
    csel_instr |= ((BR + a) << 16);   // Second source register (rA) - selected if condition is false (keeps original value)
    csel_instr |= (0x1 << 12);        // Condition code NE (not equal) is 0x1
    *p++ = csel_instr & 0xFF;
    *p++ = (csel_instr >> 8) & 0xFF;
    *p++ = (csel_instr >> 16) & 0xFF;
    *p++ = (csel_instr >> 24) & 0xFF;

    // 3 No Ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // The address of val_seq is in x15

    // // put the address of the segment we want in x9
    // // ldr x9, [x15, wB, UXTW #3] (UXTW #3 -> multiply by 8 bytes for pointers)
    // *p++ = 0xE9;
    // *p++ = 0x59;
    // *p++ = 0x60 + (BR + b);
    // *p++ = 0xf8;

    // updating this to be x28

    // ldr x9, [x28, 20, uxtw #3]
    *p++ = 0x89;
    *p++ = 0x5B;
    *p++ = 0x60 + (BR + b);
    *p++ = 0xF8;

    // load the value from the index in the target segment to register wA
    // ldr wA, [x9, wC, UXTW #2] (UXTW #2 -> multiply by 4 bytes for uint32_t)
    *p++ = 0x20 + (BR + a); // dest register
    *p++ = 0x59;
    *p++ = 0x60 + (BR + c); // using C as an index
    *p++ = 0xb8;

    // 3 No Ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // load the address of the segment we want into x9
    // // ldr x9, [x15, wA, UXTW #3] (UXTW #3 -> multiply by 8 bytes for pointers)
    // *p++ = 0xE9;
    // *p++ = 0x59;
    // *p++ = 0x60 + (BR + a);
    // *p++ = 0xf8;

    // ldr x9, [x28, 20, uxtw #3]
    *p++ = 0x89;
    *p++ = 0x5B;
    *p++ = 0x60 + (BR + a);
    *p++ = 0xF8;

    // store what we want to store at the right address
    // str wC, [x9, wB, uxtw #2] (UXTW #2 -> multiply by 4 bytes for uint32_t)
    *p++ = 0x20 + (BR + c);
    *p++ = 0x59;
    *p++ = 0x20 + (BR + b);
    *p++ = 0xB8;


    // 3 No Ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // add wA, wB, wC
    uint32_t add_instr = 0x0B000000;
    add_instr |= (BR + a);
    add_instr |= ((BR + b) << 5);
    add_instr |= ((BR + c) << 16);

    *p++ = add_instr & 0xFF;
    *p++ = (add_instr >> 8) & 0xFF;
    *p++ = (add_instr >> 16) & 0xFF;
    *p++ = (add_instr >> 24) & 0xFF;

    // 5 No ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // wA = wB x wC
    uint32_t mul_instr = 0x1B000000; // Base opcode for MUL instruction
    mul_instr |= (BR + a);           // Destination register Rd
    mul_instr |= ((BR + b) << 5);    // First source register Rn
    mul_instr |= ((BR + c) << 16);   // Second source register Rm
    mul_instr |= (0x1F << 10);       // The "1F" in bits 10-14 is part of MUL encoding

    // Write the instruction bytes in little-endian order
    *p++ = mul_instr & 0xFF;
    *p++ = (mul_instr >> 8) & 0xFF;
    *p++ = (mul_instr >> 16) & 0xFF;
    *p++ = (mul_instr >> 24) & 0xFF;

    // 5 No ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // wA = wB / wC
    // udiv wA, wB, wC
    uint32_t udiv_instr = 0x1AC00800; // Base opcode for UDIV instruction
    udiv_instr |= (BR + a);           // Destination register Rd
    udiv_instr |= ((BR + b) << 5);    // Dividend register Rn
    udiv_instr |= ((BR + c) << 16);   // Divisor register Rm

    // Write the instruction bytes in little-endian order
    *p++ = udiv_instr & 0xFF;
    *p++ = (udiv_instr >> 8) & 0xFF;
    *p++ = (udiv_instr >> 16) & 0xFF;
    *p++ = (udiv_instr >> 24) & 0xFF;

    // 5 No ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // wA = NOT(wB AND wC)

    // AND w9, wB, wC  (using w9 as temporary)
    uint32_t and_instr = 0x0A000000; // Base opcode for AND instruction
    and_instr |= 9;                  // w9 as destination (not using BR offset)
    and_instr |= ((BR + b) << 5);    // First source register
    and_instr |= ((BR + c) << 16);   // Second source register

    // Write the AND instruction
    *p++ = and_instr & 0xFF;
    *p++ = (and_instr >> 8) & 0xFF;
    *p++ = (and_instr >> 16) & 0xFF;
    *p++ = (and_instr >> 24) & 0xFF;

    // MVN wA, w9 (Move Not - bitwise NOT)
    uint32_t mvn_instr = 0x2A2003E0; // Base opcode for MVN (NOT)
    mvn_instr |= (BR + a);           // Destination register
    mvn_instr |= (9 << 16);          // Source register (w9)

    // Write the MVN instruction
    *p++ = mvn_instr & 0xFF;
    *p++ = (mvn_instr >> 8) & 0xFF;
    *p++ = (mvn_instr >> 16) & 0xFF;
    *p++ = (mvn_instr >> 24) & 0xFF;

    // 3 No Ops
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t handle_halt(void *zero, size_t offset)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov x27, #0
    *p++ = 0x1B;
    *p++ = 0x00;
    *p++ = 0x80;
    *p++ = 0xD2;

    // ret
    *p++ = 0xC0;
    *p++ = 0x03;
    *p++ = 0x5F;
    *p++ = 0xD6;

    return CHUNK;
}

uint32_t map_segment(uint32_t size)
{
    uint32_t new_seg_id;

    // If there are no available recycled segment ids, make a new one
    if (gs.rec_size == 0)
    {
        // Expand if necessary
        if (gs.seq_size == gs.seq_cap)
        {
            // print_x28();
            gs.seq_cap *= 2;

            // realloc the array that keeps track of sequence size
            gs.seg_lens = realloc(gs.seg_lens, gs.seq_cap * sizeof(uint32_t));
            assert(gs.seg_lens != NULL);

            // also need to init the memory segment
            gs.val_seq = realloc(gs.val_seq, gs.seq_cap * sizeof(uint32_t *));
            assert(gs.val_seq != NULL);

            // Initializing all reallocated memory
            for (uint32_t i = gs.seq_size; i < gs.seq_cap; i++)
            {
                gs.val_seq[i] = NULL;
                gs.seg_lens[i] = 0;
            }
        }

        new_seg_id = gs.seq_size++;
    }

    // If there are available recycled segment IDs, use one
    else
    {
        new_seg_id = gs.rec_ids[--gs.rec_size];
    }

    // If the segment didn't previously exist or wasn't large enough
    if (gs.val_seq[new_seg_id] == NULL || size > gs.seg_lens[new_seg_id])
    {
        gs.val_seq[new_seg_id] = realloc(gs.val_seq[new_seg_id], size * sizeof(uint32_t));
        assert(gs.val_seq[new_seg_id] != NULL);

        gs.seg_lens[new_seg_id] = size;
    }

    // zero out the new segment
    memset(gs.val_seq[new_seg_id], 0, size * sizeof(uint32_t));

    // Using x16, an intraprocedure call temporary register, to store the new
    // value of the value sequence
    __asm__ volatile (
        "mov x16, %0"
        : 
        : "r"(gs.val_seq)
        : "x16"
    );

    return new_seg_id;
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Move register c to be the function call argument
    // mov w0, wC
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = BR + c;
    *p++ = 0x2A;

    // call map segment function

    // Save x30 to stack
    *p++ = 0xFE; // str x30, [sp, #-16]!
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0xF8;

    // since this call is common, we are giving it it's own register to play
    // with since Arm is rich in registers and we want things to go fast
    // This one does not need to be changed
    // blr x12
    *p++ = 0x80;
    *p++ = 0x01;
    *p++ = 0x3F;
    *p++ = 0xD6;

    // Restore x30 from stack
    *p++ = 0xFE; // ldr x30, [sp], #16
    *p++ = 0x07;
    *p++ = 0x41;
    *p++ = 0xF8;

    // mov result into reg c
    // mov return value from x0 to wB
    *p++ = 0xE0 + (BR + b);
    *p++ = 0x03;
    *p++ = 0x00;
    *p++ = 0x2A;

    return CHUNK;
}

void unmap_segment(uint32_t segmentId)
{
    if (gs.rec_size == gs.rec_cap)
    {
        gs.rec_cap *= 2;
        gs.rec_ids = realloc(gs.rec_ids, gs.rec_cap * sizeof(uint32_t));
    }

    gs.rec_ids[gs.rec_size++] = segmentId;
}

size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Move register c to be the function call argument
    // mov w0, wC
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = BR + c;
    *p++ = 0x2A;

    // load correct opcode
    // Move enum code for print reg into w1
    // mov w1, OP_OUT
    uint32_t mov = 0x52800000;
    mov |= (OP_UNMAP & 0xFFFF) << 5;
    mov |= OP_REG;

    *p++ = mov & 0xFF;
    *p++ = (mov >> 8) & 0xFF;
    *p++ = (mov >> 16) & 0xFF;
    *p++ = (mov >> 24) & 0xFF;

    // call unmap segment function

    // Save x30 to stack
    *p++ = 0xFE; // str x30, [sp, #-16]!
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0xF8;

    // This needs to be changed, probably to blr x15
    // blr x15
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x3F;
    *p++ = 0xD6;

    // // blr x28
    // *p++ = 0x80;
    // *p++ = 0x03;
    // *p++ = 0x3F;
    // *p++ = 0xD6;

    // Restore x30 from stack
    *p++ = 0xFE; // ldr x30, [sp], #16
    *p++ = 0x07;
    *p++ = 0x41;
    *p++ = 0xF8;

    return CHUNK;
}

size_t print_reg(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov w0, wC
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = BR + c;
    *p++ = 0x2A;

    // Move enum code for print reg into w1
    // mov w1, OP_OUT
    uint32_t mov = 0x52800000;
    mov |= (OP_OUT & 0xFFFF) << 5;
    mov |= OP_REG;

    *p++ = mov & 0xFF;
    *p++ = (mov >> 8) & 0xFF;
    *p++ = (mov >> 16) & 0xFF;
    *p++ = (mov >> 24) & 0xFF;

    // save current instruction pointer (+8 bytes) to x13
    // adr x13, +8
    *p++ = 0x4D;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x10;

    // change this to br x15
    // br x15
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x1F;
    *p++ = 0xD6;

    // // br x28
    // *p++ = 0x80;
    // *p++ = 0x03;
    // *p++ = 0x1F;
    // *p++ = 0xD6;

    // 2 No op
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t read_into_reg(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Move enum code for read reg into w1
    // mov w1, OP_OUT
    uint32_t mov = 0x52800000;
    mov |= (OP_IN & 0xFFFF) << 5;
    mov |= OP_REG;

    *p++ = mov & 0xFF;
    *p++ = (mov >> 8) & 0xFF;
    *p++ = (mov >> 16) & 0xFF;
    *p++ = (mov >> 24) & 0xFF;

    // save current instruction pointer to x13
    // adr x13, +8
    *p++ = 0x4D;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x10;

    // change this to br x15
    // br x15
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x1F;
    *p++ = 0xD6;

    // // br x28
    // *p++ = 0x80;
    // *p++ = 0x03;
    // *p++ = 0x1F;
    // *p++ = 0xD6;

    // mov result into reg c
    *p++ = 0xE0 + (BR + c);
    *p++ = 0x03;
    *p++ = 0x00;
    *p++ = 0x2A;

    // 2 no op
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

void *load_program(uint32_t b_val)
{
    // printf("Load program was called\n");
    /* The inline assembly for the load program sets the program counter gs.pc
     * and returns the correct address is b_val is 0.
     * This function handles loading a non-zero segment into segment zero. */
    assert(b_val != 0);

    uint32_t new_seg_size = gs.seg_lens[b_val];
    uint32_t *new_vals = calloc(new_seg_size, sizeof(uint32_t));
    memcpy(new_vals, gs.val_seq[b_val], new_seg_size * sizeof(uint32_t));

    /* Update the existing memory segment */
    gs.val_seq[0] = new_vals;
    gs.seg_lens[0] = new_seg_size;

    // Allocate new executable memory for the segment being mapped
    void *new_zero = mmap(NULL, new_seg_size * CHUNK,
                          PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(new_zero, 0, new_seg_size * CHUNK);

    // Compile the segment being mapped into machine instructions
    uint32_t offset = 0;
    for (uint32_t i = 0; i < new_seg_size; i++)
    {
        offset = compile_instruction(new_zero, new_vals[i], offset);
    }

    gs.active = new_zero;
    printf("Finishing load program\n");
    return new_zero;
}

size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    (void)b;
    uint8_t *p = (uint8_t *)zero + offset;

    // mov the 32-bit program counter into x10
    // mov x10, wC
    *p++ = 0xEA;
    *p++ = 0x03;
    *p++ = 0x00 + (BR + c);
    *p++ = 0x2A;

    // // check if the segment being loaded is segment 0
    // // if wB is 0, jump straight to the return
    // // cbnz wB, +8
    *p++ = 0x40 + (BR + b);
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x35;

    // if yes, just return
    // ret
    *p++ = 0xC0;
    *p++ = 0x03;
    *p++ = 0x5F;
    *p++ = 0xD6;

    // mov w0, wB
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = 0x00 + (BR + b);
    *p++ = 0x2A;

    // // br x28 (We will do the return from the inline assembly)
    // *p++ = 0x80;
    // *p++ = 0x03;
    // *p++ = 0x1F;
    // *p++ = 0xD6;

    // change this to br x15
    // br x15
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x1F;
    *p++ = 0xD6;

    return CHUNK;
}
