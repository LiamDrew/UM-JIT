/** 
 * @file jit.c
 * @author Liam Drew
 * @date January 2025
 * @brief 
 * A Just-In-Time compiler from Universal Machine assembly language to
 * x86 assembly language. Uses the Virt32 memory allocator.
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

/* Set this to 1 to handle recompiling. This flag will majorly throttle the
 * segmented store instruction by design: the entire program was designed around
 * the assumption this feature would not be implemented. */
#define SELF_MODIFYING 0

typedef uint32_t Instruction;
typedef void *(*Function)(void);

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

    /* Initialize executable and non-executable memory for the zero segment */
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
            
            set_at(umem, 0 + (i / 4) * sizeof(uint32_t), word);

            /* Compile the UM word into machine code */
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

    /* Load Value */
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

    /* Output */
    if (opcode == 10)
    {
        offset += print_reg(zero, offset, c);
    }

    /* Addition */
    else if (opcode == 3)
    {
        offset += add_regs(zero, offset, a, b, c);
    }

    /* Halt */
    else if (opcode == 7)
    {
        offset += handle_halt(zero, offset);
    }

    /* Bitwise NAND */
    else if (opcode == 6)
    {
        offset += nand_regs(zero, offset, a, b, c);
    }

    /* Addition */
    else if (opcode == 3)
    {
        offset += add_regs(zero, offset, a, b, c);
    }

    /* Multiplication */
    else if (opcode == 4)
    {
        offset += mult_regs(zero, offset, a, b, c);
    }

    /* Division */
    else if (opcode == 5)
    {
        offset += div_regs(zero, offset, a, b, c);
    }

    /* Conditional Move */
    else if (opcode == 0)
    {
        offset += cond_move(zero, offset, a, b, c);
    }

    /* Input */
    else if (opcode == 11)
    {
        offset += read_into_reg(zero, offset, c);
    }

    /* Segmented Load */
    else if (opcode == 1)
    {
        offset += seg_load(zero, offset, a, b, c);
    }

    /* Segmented Store */
    else if (opcode == 2)
    {
        offset += seg_store(zero, offset, a, b, c);
    }

    /* Load Program */
    else if (opcode == 12)
    {
        offset += inject_load_program(zero, offset, b, c);
    }

    /* Map Segment */
    else if (opcode == 8)
    {
        offset += inject_map_segment(zero, offset, b, c);
    }

    /* Unmap Segment */
    else if (opcode == 9)
    {
        offset += inject_unmap_segment(zero, offset, c);
    }

    /* Invalid Opcode */
    else
    {
        offset += CHUNK;
    }

    return offset;
}

size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Load 32 bit value into register rAd */
    /* mov imm32, %rAd */
    *p++ = 0x41;
    *p++ = 0xC7;
    *p++ = 0xC0 | a;

    *p++ = value & 0xFF;
    *p++ = (value >> 8) & 0xFF;
    *p++ = (value >> 16) & 0xFF;
    *p++ = (value >> 24) & 0xFF;

    /* No Op */
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* test %rCd, %rCd */
    *p++ = 0x45;
    *p++ = 0x85;
    *p++ = 0xc0 | (c << 3) | c;

    /* If rCd is not 0, move %rAd into %rBd */
    /* cmovne %rBd, %rAd */
    *p++ = 0x45;
    *p++ = 0x0F;
    *p++ = 0x45;
    *p++ = 0xC0 | (a << 3) | b;

    /* No Op */
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}


size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* rA = m[rB][rC]*/

    /* lea (%rcx, %rBd, 1), %rax */
    *p++ = 0x4a;
    *p++ = 0x8d;
    *p++ = 0x04;
    *p++ = 0x01 | (b << 3);

    /* mov %rAd, (%rax, %rCd,4) */
    *p++ = 0x46;
    *p++ = 0x8B;
    *p++ = 0x04 | (a << 3);
    *p++ = 0x80 | (c << 3);

    /* No Ops */
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
}

size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* m[rA][rB] = rC */

    /* lea (%rcx, %rBd, 1), %rax */
    *p++ = 0x4a;
    *p++ = 0x8d;
    *p++ = 0x04;
    *p++ = 0x01 | (a << 3);

    /* mov (%rax, %rBd, 4), %rCd */
    *p++ = 0x46;
    *p++ = 0x89;
    *p++ = 0x04 | (c << 3);
    *p++ = 0x80 | (b << 3);

    /* Super hacky method for handling recompile: use the output of the lea
     * instruction as an else case: the Virt32 memory allocator always
     * allocates along 32-byte boundaries, which means that the lower 5 bits
     * will always be 0. This means that with opcodes 1-31 will be safe to use
     * for special cases, and the 0 or >32 opcodes can be used for the recompile
     * instruction. */
    if (SELF_MODIFYING) {
        /* Unconditionally branch to the handler
         * This is super inefficient, and has been implemented somewhat as an
         * afterthought. */
        *p++ = 0xff;
        *p++ = 0xd3;
    }

    else {
        /* 2 No Ops */
        *p++ = 0x90;
        *p++ = 0x90;
    }

    return CHUNK;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* rA = rB + rC % 2^32 */
    /* mov %rBd, %eax */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    /* add %rCd, %eax */
    *p++ = 0x44;
    *p++ = 0x01;
    *p++ = 0xC0 | (c << 3);

    /* mov %eax, %rAd */
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    /* No Op */
    *p++ = 0x90;

    return CHUNK;
}


size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* mov %rBd, %eax */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    /* mul %rCd, %eax */
    *p++ = 0x41;
    *p++ = 0xF7;
    *p++ = 0xE0 | c;

    /* mov %eax, %rAd */
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    /* No Op */
    *p++ = 0x90;

    return CHUNK;
}


size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* xor %edx, %edx */
    *p++ = 0x31;
    *p++ = 0xd2;

    /* Put the dividend (register b) in %eax */
    /* mov %rBd, %eax */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    /* div %rC, %rax */
    *p++ = 0x49;
    *p++ = 0xF7;
    *p++ = 0xF0 | c;

    /* Exchange %eax with the target destination register */
    /* xchg %eax, %rAd */
    *p++ = 0x41;
    *p++ = 0x90 | a;

    return CHUNK;
}

size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Thank you to Tom Hebb for figuring out this clever approach for saving a
     * an instruction.
     * When r1 = r0 NAND r1, or r1 = r1 NAND r0, we want to use the source
     * register that is the same as the destination register as the destination,
     * and be sure to avoid clobbering the other source register. 
     * For instructions where all 3 registers are the same or are different,
     * this is not a concern.
     */

    unsigned move, keep;
    if (a == c) {
        move = c;
        keep = b;
    }

    else {
        move = b;
        keep = c;
    }


    /* mov %(Move register), %rAd, */
    *p++ = 0x45;
    *p++ = 0x8b;
    *p++ = 0xc0 | (a << 3) | move;

    /* and %(Keep register), %rAd */
    *p++ = 0x45;
    *p++ = 0x23;
    *p++ = 0xc0 | (a << 3) | keep;

    /* not %rAd */
    *p++ = 0x41;
    *p++ = 0xf7;
    *p++ = 0xd0 | a;

    /* No Op */
    *p++ = 0x90;

    return CHUNK;
}

size_t handle_halt(void *zero, size_t offset)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Move the Halt opcode into %al */
    /* mov imm8, %al */
    *p++ = 0xb0;
    *p++ = 0x00 | OP_HALT;

    /* Jump to large op function address (NOTE: jump, not call) */
    /* jmp *%rbx */
    *p++ = 0xff;
    *p++ = 0xe3;

    return CHUNK;
}

uint32_t map_segment(uint32_t size, uint8_t *umem)
{
    uint32_t mapped = vs_calloc(umem, size * sizeof(uint32_t));
    return mapped;
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Move register c to be the function call argument */
    /* mov %rCd, %edi */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3);
    
    /* Move the Map opcode into %al */
    /* mov imm8, %al */
    *p++ = 0xb0;
    *p++ = 0x00 | OP_MAP;

    /* Call the large op function handler */
    /* call *%rbx */
    *p++ = 0xff;
    *p++ = 0xd3;

    /* Move return value from %rax to register b */
    /* mov %rax, %rBd */
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | b;

    return CHUNK;
}

void unmap_segment(uint32_t segment)
{
    vs_free(segment);
}

size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Move register c to be the function argument */
    /* mov %rCd, %edi */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3);

    /* Move the Unmap opcode into %al */
    /* mov imm8, %al */
    *p++ = 0xb0;
    *p++ = 0x00 | OP_UNMAP;

    /* Call the large op function handler */
    /* call *%rbx */
    *p++ = 0xff;
    *p++ = 0xd3;

    /* No Op */
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

size_t print_reg(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Move register c to be the function argument */
    /* mov %rCd, %edi */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3);

    /* Move the Print Out opcode into %al */
    /* mov imm8, %al */
    *p++ = 0xb0;
    *p++ = 0x00 | OP_OUT;

    /* Call the large op function handler */
    /* call *%rbx */
    *p++ = 0xff;
    *p++ = 0xd3;

    /* No Op */
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

size_t read_into_reg(void *zero, size_t offset, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* Move the Read In opcode into %al */
    /* mov imm8, %al */
    *p++ = 0xb0;
    *p++ = 0x00 | OP_IN;

    /* Call the large op function handler */
    /* call *%rbx */
    *p++ = 0xff;
    *p++ = 0xd3;

    /* Store the result in register c */
    /* mov %eax, %rCd */
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | c;

    /* No Op */
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

void *load_program(uint32_t b_val, uint8_t *umem)
{
    /* Ensure the segment we are loading is not the zero segment */
    assert(b_val != 0);

    /* Get the size of the segment we want to duplicate */
    uint32_t *seg_addr = (uint32_t *)convert_address(umem, b_val);
    uint32_t copy_size = seg_addr[-1];

    uint32_t num_words = copy_size / sizeof(uint32_t);

    /* Reallocate the kernel size and copy the new segment into it */
    kern_realloc(copy_size);
    kern_memcpy(b_val, copy_size);

    /* Allocate new exectuable memory for the segment being mapped
     * Note that copy size is in bytes, not words*/
    void *new_zero = mmap(NULL, num_words * CHUNK,
                          PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(new_zero, 0, num_words * CHUNK);

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

size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    uint8_t *p = (uint8_t *)zero + offset;

    /* mov %rCd, %esi (updating the program counter) */
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (c << 3);

    /* mov %rBd, %edi (to test with a known register in assembly)*/
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    /* Move the Duplicate segment opcode into %al */
    /* mov imm8, %al */
    *p++ = 0xb0;
    *p++ = 0x00 | OP_DUPLICATE;

    /* jump to load program function address (NOTE: jump, not call) */
    /* jmp *%rbx */
    *p++ = 0xff;
    *p++ = 0xe3;

    return CHUNK;
}
