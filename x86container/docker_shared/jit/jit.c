/** 
 * @file jit.c
 * @author Liam Drew
 * @date January 2025
 * @brief 
 * A Just-In-Time compiler from Universal Machine assembly language to
 * x86 assembly language.
 * 
 * This JIT completes the sandmark in 1.02 seconds in an x86 docker container
 * running on an Apple Silicon Mac. It is nearly 3 times faster than the
 * benchmark emulator.
*/

#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"

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

void initialize_instruction_bank();
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
        fprintf(stderr, "Usage: ./um [executable.um]\n");
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
    run(curr_seg);

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

    // Load 32 bit value into register rA
    // mov(32) rAd, imm32
    *p++ = 0x41;
    *p++ = 0xC7;
    *p++ = 0xC0 | a;

    *p++ = value & 0xFF;
    *p++ = (value >> 8) & 0xFF;
    *p++ = (value >> 16) & 0xFF;
    *p++ = (value >> 24) & 0xFF;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // if rC != 0, rA = rB
    // cmp rCd, 0
    *p++ = 0x41;
    *p++ = 0x83;
    *p++ = 0xF8 | c;
    *p++ = 0x00;

    // je (jump equal) 3 bytes
    *p++ = 0x74;
    *p++ = 0x03;

    // mov rAd, rBd
    *p++ = 0x45;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3) | a;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

// inject segmented load
size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Load address of val_seq into rax
    // mov(64) rax, imm64
    *p++ = 0x48;
    *p++ = 0xB8;
    uint64_t addr = (uint64_t)&gs.val_seq;
    memcpy(p, &addr, sizeof(addr));
    p += 8;

    // mov rax, [rax]
    *p++ = 0x48;
    *p++ = 0x8B;
    *p++ = 0x00;

    // mov rax, [rax + rBd*8]
    *p++ = 0x4A;                    // REX prefix: REX.W and REX.X
    *p++ = 0x8B;                    // MOV opcode
    *p++ = 0x04;                    // ModRM byte for SIB
    *p++ = 0xC0 | (b << 3); // SIB: scale=3 (8), index=B's lower bits, base=rax

    // mov rAd, [rax + rCd*4]
    *p++ = 0x46;                    // REX prefix: REX.R and REX.X
    *p++ = 0x8B;                    // MOV opcode
    *p++ = 0x04 | (a << 3);         // ModRM byte with register selection (a in reg field for destination)
    *p++ = 0x80 | (c << 3); // SIB: scale=2 (4), index=C's lower bits, base=rax

    return CHUNK;
}

/*
 * NOTE: This JIT is not configured to handle a self-modifying UM program
 * In order to do this, this segmented store compilation function would need to
 * be updated so that it compiles any value loaded into the zero segment into
 * machine code. This requires an inline function call to a C function, which
 * slows the program down. This implementation omits such a call, but 
 * INSERT OTHER VERSION HERE handles this.
 */
size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Load address of val_seq into rax
    // mov(64) rax, imm64
    *p++ = 0x48;
    *p++ = 0xB8;
    uint64_t addr = (uint64_t)&gs.val_seq;
    memcpy(p, &addr, sizeof(addr));
    p += sizeof(addr);

    // mov rax, [rax]
    *p++ = 0x48;
    *p++ = 0x8B;
    *p++ = 0x00;

    // mov rax, [rax + rAd*8]
    *p++ = 0x4A;                    // REX prefix: REX.W and REX.X
    *p++ = 0x8B;                    // MOV opcode
    *p++ = 0x04;                    // ModRM byte for SIB
    *p++ = 0xC0 | (a << 3); // SIB: scale=3 (8), index=A's lower bits, base=rax

    // mov [rax + rBd*4], rCd
    *p++ = 0x46;                    // REX prefix: REX.R and REX.X
    *p++ = 0x89;                    // MOV opcode
    *p++ = 0x04 | (c << 3);         // ModRM byte with register selection
    *p++ = 0x80 | (b << 3); // SIB: scale=2 (4), index=B's lower bits, base=rax

    return CHUNK;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    // add eax, rCd
    *p++ = 0x44;
    *p++ = 0x01;
    *p++ = 0xC0 | (c << 3);

    // mov rAd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    
    return CHUNK;
}

size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    // mul eax, rCd
    *p++ = 0x41;
    *p++ = 0xF7;
    *p++ = 0xE0 | c;

    // mov rAd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // xor rdx, rdx
    *p++ = 0x48;
    *p++ = 0x31;
    *p++ = 0xD2;

    // put the dividend (reg b) in eax
    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    // div rax, rC
    *p++ = 0x49;
    *p++ = 0xF7;
    *p++ = 0xF0 | c;

    // mov rAd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;

    return CHUNK;
}

size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc0 | (b << 3);

    // and eax, rcd
    *p++ = 0x44;
    *p++ = 0x21;
    *p++ = 0xc0 | (c << 3);

    // not eax
    *p++ = 0x40;
    *p++ = 0xf7;
    *p++ = 0xd0;

    // mov rAd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | a;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

size_t handle_halt(void *zero, size_t offset)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Set rax to 0 (NULL);
    // xor rax,rax
    *p++ = 0x48;
    *p++ = 0x31;
    *p++ = 0xc0;
    *p++ = 0xc3; // return

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

    return new_seg_id;
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Move register c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3);

    // call map segment
    *p++ = 0xb0;
    *p++ = 0x00 | OP_MAP;

    *p++ = 0xff;
    *p++ = 0xd3;

    // move return value from rax to reg b
    // mov rBd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | b;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;
    *p++ = 0x90;

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
    // mov edi, rCd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3);

    // load correct opcode
    *p++ = 0xb0;
    *p++ = 0x00 | OP_UNMAP;

    // call unmap segment function
    *p++ = 0xff;
    *p++ = 0xd3;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
}

size_t print_reg(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov edi, rCd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3);

    // load immediate value into al
    *p++ = 0xb0;
    *p++ = 0x00 | OP_OUT;

    // Jump to address in rbx
    *p++ = 0xff;
    *p++ = 0xd3;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
}

size_t read_into_reg(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // put the right opcode into rax
    *p++ = 0xb0;
    *p++ = 0x00 | OP_IN;

    // call the function
    *p++ = 0xff;
    *p++ = 0xd3;

    // mov rCd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | c;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
}

void *load_program(uint32_t b_val)
{
    // This function handles loading a non-zero segment into segment zero
    uint32_t new_seg_size = gs.seg_lens[b_val];
    uint32_t *new_vals = calloc(new_seg_size, sizeof(uint32_t));
    memcpy(new_vals, gs.val_seq[b_val], new_seg_size * sizeof(uint32_t));

    // Update the existing memory segment
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
    return new_zero;
}

// fast version
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // mov rsi, rCd (updating the program counter)
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (c << 3);

    // mov %rbp, %rax
    *p++ = 0x48;
    *p++ = 0x89;
    *p++ = 0xe8;

    // test %rBd, %rBd
    *p++ = 0x45;
    *p++ = 0x85;
    *p++ = 0xc0 | (b << 3) | b;

    // jne
    *p++ = 0x75;
    *p++ = 0x01;

    *p++ = 0xc3;

    // It makes zero sense that this program works without this instruction
    // mov edi, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    // call load program
    *p++ = 0xb0;
    *p++ = 0x00 | OP_DUPLICATE;

    *p++ = 0xff;
    *p++ = 0xd3;

    // return (correct value is already in rax from load_program_addr)
    *p++ = 0xc3;

    return CHUNK;
}

// // small version (18 bytes)
// size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
// {
//     uint8_t *p = (uint8_t *)zero + offset;

//     // mov rsi, rCd (updating the program counter)
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc6 | (c << 3);

//     // mov %rbp, %rax
//     *p++ = 0x48;
//     *p++ = 0x89;
//     *p++ = 0xe8;

//     // It makes zero sense that this program works without this instruction
//     // mov edi, rBd
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc7 | (b << 3);

//     // test %edi, %edi  (test if b_val is 0)
//     *p++ = 0x85;
//     *p++ = 0xff;

//     // je
//     *p++ = 0x74;
//     *p++ = 0x04;

//     // call load program
//     *p++ = 0xb0;
//     *p++ = 0x00 | OP_DUPLICATE;

//     *p++ = 0xff;
//     *p++ = 0xd3;

//     // return (correct value is already in rax from load_program_addr)
//     *p++ = 0xc3;

//     return CHUNK;
// }

// Useful for debugging:
// size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
// {
//     uint8_t *p = (uint8_t *)zero + offset;

//     // mov rsi, rCd (updating the program counter)
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc6 | (c << 3);

//     // // mov(64) rax, imm64
//     // *p++ = 0x48;
//     // *p++ = 0xb8;
//     // uint64_t addr = (uint64_t)&gs.active;
//     // memcpy(p, &addr, sizeof(uint64_t));
//     // p += sizeof(uint64_t);

//     // // // Just read from the address directly
//     // // mov rax, [rax]
//     // *p++ = 0x48;
//     // *p++ = 0x8b;
//     // *p++ = 0x00; // ModRM byte for [rax] with no offset

//     // mov %rbp, %rax
//     *p++ = 0x48;
//     *p++ = 0x89;
//     *p++ = 0xe8;

//     // // Calling debug function
//     // // call debug
//     // *p++ = 0xb0;
//     // *p++ = 0x00 | 6;

//     // // call function
//     // *p++ = 0xff;
//     // *p++ = 0xd3;

//     // test %rBd, %rBd
//     *p++ = 0x45;
//     *p++ = 0x85;
//     *p++ = 0xc0 | (b << 3) | b;

//     // je
//     *p++ = 0x74;
//     *p++ = 0x07;

//     // It makes zero sense that this program works without this instruction
//     // mov edi, rBd
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc7 | (b << 3);

//     // call load program
//     *p++ = 0xb0;
//     *p++ = 0x00 | OP_DUPLICATE;

//     *p++ = 0xff;
//     *p++ = 0xd3;

//     // return (correct value is already in rax from load_program_addr)
//     *p++ = 0xc3;

//     return CHUNK;
// }
