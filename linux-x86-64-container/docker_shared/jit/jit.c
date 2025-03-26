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

#include "virt.h"

#define OPS 15
#define INIT_CAP 32500

typedef uint32_t Instruction;
typedef void *(*Function)(void);

void initialize_instruction_bank();
void *initialize_zero_segment(size_t fsize);
void load_zero_segment(void *zero, uint8_t *umem, FILE *fp, size_t fsize);
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
uint32_t map_segment(uint32_t size, uint8_t *umem);
size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c);

void unmap_segment(uint32_t segmentID);
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c);

void print_out(uint32_t x);
size_t print_reg(void *zero, size_t offset, unsigned c);

unsigned char read_char(void);
size_t read_into_reg(void *zero, size_t offset, unsigned c);

// void *load_program(uint32_t b_val);
void *load_program(uint32_t b_val, uint8_t *umem);
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


    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
    {
        fsize = file_stat.st_size;
        assert((fsize % 4) == 0);
    }

    uint8_t *umem = init_memory_system(KERN_SIZE);
    printf("Umem at the beginning is %p\n", (void *)umem);

    // Initialize executable and non-executable memory for the zero segment
    size_t asmbytes = fsize * ((CHUNK + 3) / 4);
    void *zero = initialize_zero_segment(asmbytes);

    load_zero_segment(zero, umem, fp, fsize);
    fclose(fp);

    uint8_t *curr_seg = (uint8_t *)zero;
    run(curr_seg, umem);

    terminate_memory_system();

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

void load_zero_segment(void *zero, uint8_t *umem, FILE *fp, size_t fsize)
{
    kern_realloc(fsize);
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
            // zero_vals[i / 4] = word;
            
            set_at(umem, 0 + (1 / 4) * sizeof(uint32_t), word);

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

    printf("Opcode is %u\n", opcode);

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

    // no op
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // test rc, rc
    *p++ = 0x45;
    *p++ = 0x85;
    *p++ = 0xc0 | (c << 3) | c;

    // conditional move
    *p++ = 0x45;
    *p++ = 0x0F;
    *p++ = 0x45;
    *p++ = 0xC0 | (a << 3) | b;

    // no op
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

// inject segmented load
// size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
// {
//     (void)a;
//     (void)b;
//     (void)c;

//     uint8_t *p = (uint8_t *)zero + offset;

//     // // mov rax, [rcx + rBd*8]
//     // *p++ = 0x4A;            // REX prefix: REX.W and REX.X
//     // *p++ = 0x8B;            // MOV opcode
//     // *p++ = 0x04;            // ModRM byte for SIB
//     // *p++ = 0xC1 | (b << 3); // SIB: scale=3 (8), index=B's lower bits, base=rax

//     // // i am trying to sum rcx and rB, and store the result in rax
//     // *p++ = 0x4a;
//     // *p++ = 0x8d;
//     // *p++ = 0x04;
//     // *p++ = 0x01 | (b << 3);

//     // // mov %rcx,%rax
//     // *p++ = 0x48;
//     // *p++ = 0x89;
//     // *p++ = 0xc8;

//     // // add rB, rax
//     // *p++ = 0x4c;
//     // *p++ = 0x01;
//     // *p++ = 0xc0 | (b << 3);

//     // // mov rAd, [rax + rCd*4]
//     // *p++ = 0x46;                    // REX prefix: REX.R and REX.X
//     // *p++ = 0x8B;                    // MOV opcode
//     // *p++ = 0x04 | (a << 3);         // ModRM byte with register selection (a in reg field for destination)
//     // *p++ = 0x80 | (c << 3); // SIB: scale=2 (4), index=C's lower bits, base=rax

//     // $r[A] := $m[$r[B]][$r[C]]

//     *p++ = 0x4c;
//     *p++ = 0x89;
//     *p++ = 0xc6 | (b << 3);

//     // put the right opcode into rax
//     *p++ = 0xb0;
//     *p++ = 0x00 | OP_LOAD;

//     // call the function
//     *p++ = 0xff;
//     *p++ = 0xd3;

//     // *p++ = 0x0F;
//     // *p++ = 0x1F;
//     // *p++ = 0x00;
//     *p++ = 0x0F;
//     *p++ = 0x1F;
//     *p++ = 0x00;

//     *p++ = 0x0F;
//     *p++ = 0x1F;
//     *p++ = 0x00;

//     return CHUNK;
// }

size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    (void)a;
    (void)b;
    (void)c;

    uint8_t *p = (uint8_t *)zero + offset;
    // $r[A] := $m[$r[B]][$r[C]]

    // mov %rBd, %edi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    // mov %rCd, %esi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (c << 3);

    // put the right opcode into rax
    *p++ = 0xb0;
    *p++ = 0x00 | OP_LOAD;

    // call the function
    *p++ = 0xff;
    *p++ = 0xd3;

    // mov %eax, rAd
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | a;

    return CHUNK;
}

uint32_t seg_load_print(uint32_t seg_addr, uint32_t index, uint32_t garbage, uint8_t *umem)
{
    (void)garbage;
    printf("\nSEGMENTED LOAD\n");
    printf("Segment address is %u\n", seg_addr);
    printf("Segment index is %u\n", index);
    printf("Mem addr is %p\n", (void *)umem);

    uint8_t *temp = umem + seg_addr;
    uint32_t *real_addr = (uint32_t *)temp;
    // something is going wrong in memory right here
    // uint32_t result = real_addr[index];
    uint32_t result = *real_addr;
    printf("Storing the result %u\n", result);
    return result;
}

void seg_store_print(uint32_t seg_addr, uint32_t index, uint32_t value, uint8_t *umem)
{
    printf("\nSEGMENTED Store\n");
    printf("Segment address is %u\n", seg_addr);
    printf("Segment index is %u\n", index);
    printf("Value is %u\n", value);
    printf("Mem addr is %p\n", (void *)umem);

    uint8_t *temp = umem + seg_addr;
    uint32_t *real_addr = (uint32_t *)temp;
    real_addr[index] = value;
    // uint32_t x = (uint32_t)11 << 30;
    // *real_addr = x;
}

size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    (void)a;
    (void)b;
    (void)c;

    // $m[$r[A]][$r[B]] := $r[C]

    // mov %rAd, %edi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (a << 3);

    // mov %rBd, %esi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (b << 3);

    // mov %rCd, %edx
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc2 | (c << 3);

    // put the right opcode into rax
    *p++ = 0xb0;
    *p++ = 0x00 | OP_STORE;

    // call the function
    *p++ = 0xff;
    *p++ = 0xd3;

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
// size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
// {
//     uint8_t *p = (uint8_t *)zero + offset;

//     // *p++ = 0x4A;            // REX prefix: REX.W and REX.X
//     // *p++ = 0x8B;            // MOV opcode
//     // *p++ = 0x04;            // ModRM byte for SIB
//     // *p++ = 0xC1 | (a << 3); // SIB: scale=3 (8), index=B's lower bits, base=rax

//     // 
//     // *p++ = 0x4a;
//     // *p++ = 0x8d;
//     // *p++ = 0x04;
//     // *p++ = 0x01 | (a << 3);

//     // // mov %rcx,%rax
//     // *p++ = 0x48;
//     // *p++ = 0x89;
//     // *p++ = 0xc8;

//     // // add rB, rax
//     // *p++ = 0x4c;
//     // *p++ = 0x01;
//     // *p++ = 0xc0 | (a << 3);

//     // mov [rax + rBd*4], rCd
//     // *p++ = 0x46;                    // REX prefix: REX.R and REX.X
//     // *p++ = 0x89;                    // MOV opcode
//     // *p++ = 0x04 | (c << 3);         // ModRM byte with register selection
//     // *p++ = 0x80 | (b << 3); // SIB: scale=2 (4), index=B's lower bits, base=rax

//     // *p++ = 0x90;
//     // *p++ = 0x90;
//     (void)a;
//     (void)b;
//     (void)c;
//     // *p++ = 0x0F;
//     // *p++ = 0x1F;
//     // *p++ = 0x00;

//     // *p++ = 0x0F;
//     // *p++ = 0x1F;
//     // *p++ = 0x00;

//     // *p++ = 0x0F;
//     // *p++ = 0x1F;
//     // *p++ = 0x00;

//     // *p++ = 0x90;

//     // // no op
//     // *p++ = 0x90;
//     // *p++ = 0x90;

//     // $m[$r[A]][$r[B]] := $r[C]

//     *p++ = 0x4c;
//     *p++ = 0x89;
//     *p++ = 0xc6 | (a << 3);

//     // put the right opcode into rax
//     *p++ = 0xb0;
//     *p++ = 0x00 | OP_STORE;

//     // call the function
//     *p++ = 0xff;
//     *p++ = 0xd3;

//     // *p++ = 0x0F;
//     // *p++ = 0x1F;
//     // *p++ = 0x00;
//     *p++ = 0x0F;
//     *p++ = 0x1F;
//     *p++ = 0x00;

//     *p++ = 0x0F;
//     *p++ = 0x1F;
//     *p++ = 0x00;

//     return CHUNK;
// }

// TODO: use the Tom approach to make some of the 
size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // could the lea instruction be faster? I could see this being slow in
    // the container and fast on the servers. We will C
    
    // rA = rB + rC % 2^32

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

    // no op
    *p++ = 0x90;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}


// TODO: make this better
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

    // no op
    *p++ = 0x90;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}


size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // xor rdx, rdx
    // TODO: Made this better
    *p++ = 0x31;
    *p++ = 0xc2 | (2 << 3);

    // put the dividend (reg b) in eax
    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    // div rax, rC
    *p++ = 0x49;
    *p++ = 0xF7;
    *p++ = 0xF0 | c;

    // can save a byte by exchanging eax with rAd
    // insane how much faster exchanging registers is than moving
    *p++ = 0x41;
    *p++ = 0x90 | a;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}


// TODO: Improve this as well
size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // Tom's approach. Look into this more
    unsigned move, keep;
    if (a == c) {
        move = c;
        keep = b;
    }

    else {
        move = b;
        keep = c;
    }

    *p++ = 0x45;
    *p++ = 0x8b;
    *p++ = 0xc0 | (a << 3) | move;

    *p++ = 0x45;
    *p++ = 0x23;
    *p++ = 0xc0 | (a << 3) | keep;

    *p++ = 0x41;
    *p++ = 0xf7;
    *p++ = 0xd0 | a;

    *p++ = 0x90;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}


size_t handle_halt(void *zero, size_t offset)
{
    uint8_t *p = (uint8_t *)zero + offset;

    // jump to the halt handler
    *p++ = 0xb0;           // mov al, imm8
    *p++ = 0x00 | OP_HALT; // immediate value

    *p++ = 0xff;           // call prefix
    *p++ = 0xe3;           // jump *%rbx

    return CHUNK;
}

uint32_t map_segment(uint32_t size, uint8_t *umem)
{
    uint32_t mapped = vs_calloc(umem, size * sizeof(uint32_t));
    printf("Mapped segment is %u\n", mapped);
    return mapped;
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

    return CHUNK;
}

void unmap_segment(uint32_t segment)
{
    vs_free(segment);
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

    // no ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

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

    // no ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

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

    // no ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

void *load_program(uint32_t b_val, uint8_t *umem)
{
    /* Ensure the segment we are loading is not the zero segment */
    assert(b_val != 0);
    printf("b_val is %u\n", b_val);
    printf("umem is %p\n", (void *)umem);

    /* Get the size of the segment we want to duplicate */
    uint32_t *seg_addr = (uint32_t *)convert_address(umem, b_val);
    uint32_t copy_size = seg_addr[-1];

    uint32_t num_words = copy_size / sizeof(uint32_t);

    /* Reallocate the kernel size and copy the new segment into it */
    kern_realloc(copy_size);
    kern_memcpy(b_val, copy_size);

    /* Allocate new exectuable memory for the segment being mapped
     * Note that copy size is in bytes, not words*/
    // TODO: fix this 3 business
    void *new_zero = mmap(NULL, copy_size * 3,
                          PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(new_zero, 0, copy_size * 3);

    /* Compile the segment being mapped into machine instructions */
    uint32_t offset = 0;
    for (uint32_t i = 0; i < num_words; i++)
    {
        uint32_t curr_word = get_at(umem, i * sizeof(uint32_t));
        offset = compile_instruction(new_zero, curr_word, offset);
    }

    int result = mprotect(new_zero, num_words * CHUNK, PROT_READ | PROT_EXEC);
    assert(result == 0);

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

    /* Move rBd to edi as a scratch register*/
    /* mov %rBd, %edi */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    // jmp to load program
    *p++ = 0xb0;
    *p++ = 0x00 | OP_DUPLICATE;

    *p++ = 0xff;
    *p++ = 0xe3;

    return CHUNK;
}
