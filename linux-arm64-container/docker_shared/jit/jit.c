/**
 * @file jit.c
 * @author Liam Drew
 * @date March 2025
 * @brief
 * A Just-In-Time compiler from Universal Machine assembly language to
 * Arm assembly language. Uses the Virt32 memory allocator.
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

#include "virt.h"

#include <sys/time.h>

#define OPS 15
#define INIT_CAP 32500

#define SELF_MODIFYING 0   /* Set this to 1 to handle self-modifying code */

typedef uint32_t Instruction;

typedef void *(*Function)(void);

void *initialize_zero_segment(size_t fsize);
void load_zero_segment(void *zero, uint8_t *umem, FILE *fp, size_t fsize);
uint64_t make_word(uint64_t word, unsigned width, unsigned lsb, uint64_t value);

size_t compile_instruction(void *zero, uint32_t word, size_t offset);
size_t load_reg(uint8_t *p, unsigned a, uint32_t value);
size_t cond_move(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t seg_load(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t seg_store(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t add_regs(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t mult_regs(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t div_regs(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t nand_regs(uint8_t *p, unsigned a, unsigned b, unsigned c);
size_t handle_halt(uint8_t *p);
uint32_t map_segment(uint32_t size, uint8_t *umem);
size_t inject_map_segment(uint8_t *p, unsigned b, unsigned c);

void unmap_segment(uint32_t segmentID);
size_t inject_unmap_segment(uint8_t *p, unsigned c);

void print_out(uint32_t x);
size_t print_reg(uint8_t *p, unsigned c);

unsigned char read_char(void);
size_t read_into_reg(uint8_t *p, unsigned c);

void *load_program(uint32_t b_val, uint8_t *umem);
size_t inject_load_program(uint8_t *p, unsigned b, unsigned c);

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

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
    {
        fsize = file_stat.st_size;
        assert((fsize % 4) == 0);
    }

    uint8_t *umem = init_memory_system(KERN_SIZE);

    size_t asmbytes = fsize * ((CHUNK + 3) / 4);
    void *zero = initialize_zero_segment(asmbytes);

    load_zero_segment(zero, umem, fp, fsize);

    int result = mprotect(zero, asmbytes, PROT_READ | PROT_EXEC);
    assert(result == 0);

    uint8_t *curr_seg = (uint8_t *)zero;
    run(curr_seg, umem);

    terminate_memory_system();

    return 0;
}

void *initialize_zero_segment(size_t asmbytes)
{
    void *zero = mmap(NULL, asmbytes, PROT_READ | PROT_WRITE,
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

            /* Storing the UM word in the zero segment */
            set_at(umem, 0 + (i / 4) * sizeof(uint32_t), word);

            /* Compiling the UM word into machine code */
            offset = compile_instruction(zero, word, offset);
            word = 0;
        }
        i++;
    }

    fclose(fp);
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

    uint8_t *p = (uint8_t *)zero + offset;

    /* Load Value */
    if (opcode == 13)
    {
        a = (word >> 25) & 0x7;
        uint32_t val = word & 0x1FFFFFF;
        offset += load_reg(p, a, val);
        return offset;
    }

    uint32_t b = 0, c = 0;

    c = word & 0x7;
    b = (word >> 3) & 0x7;
    a = (word >> 6) & 0x7;

    /* Output */
    if (opcode == 10)
        offset += print_reg(p, c);

    /* Addition */
    else if (opcode == 3)
        offset += add_regs(p, a, b, c);

    /* Halt */
    else if (opcode == 7)
        offset += handle_halt(p);

    /* Bitwise NAND */
    else if (opcode == 6)
        offset += nand_regs(p, a, b, c);

    /* Multiplication */
    else if (opcode == 4)
        offset += mult_regs(p, a, b, c);

    /* Division */
    else if (opcode == 5)
        offset += div_regs(p, a, b, c);

    /* Conditional Move */
    else if (opcode == 0)
        offset += cond_move(p, a, b, c);

    /* Input */
    else if (opcode == 11)
        offset += read_into_reg(p, c);

    /* Segmented Load */
    else if (opcode == 1)
        offset += seg_load(p, a, b, c);

    /* Segmented Store */
    else if (opcode == 2)
        offset += seg_store(p, a, b, c);

    /* Load Program */
    else if (opcode == 12)
        offset += inject_load_program(p, b, c);

    /* Map Segment */
    else if (opcode == 8)
        offset += inject_map_segment(p, b, c);

    /* Unmap Segment */
    else if (opcode == 9)
        offset += inject_unmap_segment(p, c);

    /* Invalid Opcode */
    else
        offset += CHUNK;

    return offset;
}

size_t load_reg(uint8_t *p, unsigned a, uint32_t value)
{
    /* mov w19, 0x0000 */
    uint32_t lower_mov = 0x52800000;
    lower_mov |= (value & 0xFFFF) << 5;
    lower_mov |= (BR + a);

    *p++ = lower_mov & 0xFF;
    *p++ = (lower_mov >> 8) & 0xFF;
    *p++ = (lower_mov >> 16) & 0xFF;
    *p++ = (lower_mov >> 24) & 0xFF;

    /* movk w19, #0x0000, lsl 16 */
    uint32_t upper_mov = 0x72A00000;
    upper_mov |= ((value >> 16) & 0xFFFF) << 5;
    upper_mov |= (BR + a);

    *p++ = upper_mov & 0xFF;
    *p++ = (upper_mov >> 8) & 0xFF;
    *p++ = (upper_mov >> 16) & 0xFF;
    *p++ = (upper_mov >> 24) & 0xFF;

    /* 1 No Op */
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t cond_move(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* cmp w0, #0x0 */
    uint32_t cmp_instr = 0x7100001F;
    cmp_instr |= ((BR + c) << 5);

    *p++ = cmp_instr & 0xFF;
    *p++ = (cmp_instr >> 8) & 0xFF;
    *p++ = (cmp_instr >> 16) & 0xFF;
    *p++ = (cmp_instr >> 24) & 0xFF;

    /* csel wA, wB, wA, CONDITION (flag set above) */
    uint32_t csel_instr = 0x1A800000;
    csel_instr |= ((BR + a));
    csel_instr |= ((BR + b) << 5);
    csel_instr |= ((BR + a) << 16);
    csel_instr |= (0x1 << 12);

    *p++ = csel_instr & 0xFF;
    *p++ = (csel_instr >> 8) & 0xFF;
    *p++ = (csel_instr >> 16) & 0xFF;
    *p++ = (csel_instr >> 24) & 0xFF;

    /* 1 No Op */
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t seg_load(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* x28 contains the first usable memory address in the 32-bit address
     * space. */

    /* add x9, x28, wB */
    *p++ = 0x89;
    *p++ = 0x03;
    *p++ = 0x00 + (BR + b);
    *p++ = 0x8B;

    /* Load the value from the index in the target segment to register wA
     * ldr wA, [x9, wC, UXTW #2] (UXTW #2 -> multiply by 4 bytes for uint32) */
    *p++ = 0x20 + (BR + a); /* dest register */
    *p++ = 0x59;
    *p++ = 0x60 + (BR + c); /* using C as an index */
    *p++ = 0xb8;

    /* 1 No Op */
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

void print_wb(uint32_t b)
{
    printf("wB is %u\n", b);
}

size_t seg_store(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* add x9, x28, wA */
    *p++ = 0x89;
    *p++ = 0x03;
    *p++ = 0x00 + (BR + a);
    *p++ = 0x8B;

    /* Store register wC at the target address
     * str wC, [x9, wB, uxtw #2] (UXTW #2 -> multiply by 4 bytes for uint32) */
    *p++ = 0x20 + (BR + c);
    *p++ = 0x59;
    *p++ = 0x20 + (BR + b);
    *p++ = 0xB8;

    /* If the self-modifying flag is set, branch to the assembly handler for
     * self modifying code. This hurts program peformance. */
    if (SELF_MODIFYING) {
        /* blr x13 */
        *p++ = 0xA0;
        *p++ = 0x01;
        *p++ = 0x3F;
        *p++ = 0xD6;
    }

    else {
        /* 1 No Op */
        *p++ = 0x1F;
        *p++ = 0x20;
        *p++ = 0x03;
        *p++ = 0xD5;
    }

    return CHUNK;
}

size_t add_regs(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* add wA, wB, wC */
    uint32_t add_instr = 0x0B000000;
    add_instr |= (BR + a);
    add_instr |= ((BR + b) << 5);
    add_instr |= ((BR + c) << 16);

    *p++ = add_instr & 0xFF;
    *p++ = (add_instr >> 8) & 0xFF;
    *p++ = (add_instr >> 16) & 0xFF;
    *p++ = (add_instr >> 24) & 0xFF;

    /* 2 No Ops */
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

size_t mult_regs(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* mul wA, wB, wC -> (wA = wB x wC) */
    uint32_t mul_instr = 0x1B000000; /* Base opcode for mul instruction */
    mul_instr |= (BR + a);           /* Destination register */
    mul_instr |= ((BR + b) << 5);    /* First source register */
    mul_instr |= ((BR + c) << 16);   /* Second source register */
    mul_instr |= (0x1F << 10);

    *p++ = mul_instr & 0xFF;
    *p++ = (mul_instr >> 8) & 0xFF;
    *p++ = (mul_instr >> 16) & 0xFF;
    *p++ = (mul_instr >> 24) & 0xFF;

    /* 2 No Ops */
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

size_t div_regs(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* udiv wA, wB, wC -> (wA = wB / wC) */

    uint32_t udiv_instr = 0x1AC00800; /* Base opcode for udiv instruction */
    udiv_instr |= (BR + a);           /* Destination register */
    udiv_instr |= ((BR + b) << 5);    /* Dividend register */
    udiv_instr |= ((BR + c) << 16);   /* Divisor register */

    *p++ = udiv_instr & 0xFF;
    *p++ = (udiv_instr >> 8) & 0xFF;
    *p++ = (udiv_instr >> 16) & 0xFF;
    *p++ = (udiv_instr >> 24) & 0xFF;

    /* 2 No Ops */
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

size_t nand_regs(uint8_t *p, unsigned a, unsigned b, unsigned c)
{
    /* wA = NOT(wB AND wC) */

    /* and w9, wB, wC  (using w9 as temporary) */
    uint32_t and_instr = 0x0A000000; /* Base opcode for AND instruction */
    and_instr |= 9;                  /* using w9 as a temporary destination */
    and_instr |= ((BR + b) << 5);    /* First source register */
    and_instr |= ((BR + c) << 16);   /* Second source register */

    *p++ = and_instr & 0xFF;
    *p++ = (and_instr >> 8) & 0xFF;
    *p++ = (and_instr >> 16) & 0xFF;
    *p++ = (and_instr >> 24) & 0xFF;

    /* mvn wA, w9 (Move Not - bitwise NOT) */
    uint32_t mvn_instr = 0x2A2003E0; /* Base opcode for MVN (NOT) */
    mvn_instr |= (BR + a);           /* Destination register */
    mvn_instr |= (9 << 16);          /* Source register (w9) */

    *p++ = mvn_instr & 0xFF;
    *p++ = (mvn_instr >> 8) & 0xFF;
    *p++ = (mvn_instr >> 16) & 0xFF;
    *p++ = (mvn_instr >> 24) & 0xFF;

    /* 1 No Op */
    *p++ = 0x1F;
    *p++ = 0x20;
    *p++ = 0x03;
    *p++ = 0xD5;

    return CHUNK;
}

size_t handle_halt(uint8_t *p)
{
    /* Move the halt opcode into w1
     * mov w1, OP_HALT */
    *p++ = 0x00 | (OP_HALT << 5) | OP_REG;
    *p++ = 0x00;
    *p++ = 0x80;
    *p++ = 0x52;

    /* br x15 */
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x1F;
    *p++ = 0xD6;

    return CHUNK;
}

uint32_t map_segment(uint32_t size, uint8_t *umem)
{
    return vs_calloc(umem, size * sizeof(uint32_t));
}

size_t inject_map_segment(uint8_t *p, unsigned b, unsigned c)
{
    /* Move register c to be the function call argument */
    /* mov w0, wC */
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = (BR + c);
    *p++ = 0x2A;

    /* Call map segment function */
    /* blr x12 */
    *p++ = 0x80;
    *p++ = 0x01;
    *p++ = 0x3F;
    *p++ = 0xD6;

    /* Mov return value from w0 to wB:
     * mov wB, w0 */
    *p++ = 0xE0 + (BR + b);
    *p++ = 0x03;
    *p++ = 0x00;
    *p++ = 0x2A;

    return CHUNK;
}

void unmap_segment(uint32_t segment)
{
    vs_free(segment);
}

size_t inject_unmap_segment(uint8_t *p, unsigned c)
{
    /* Move register c to be the function call argument
     * mov w0, wC */
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = (BR + c);
    *p++ = 0x2A;

    /* Move the print register opcode into w1 */
    /* mov w1, OP_OUT */
    *p++ = 0x00 | (OP_UNMAP << 5) | OP_REG;
    *p++ = 0x00;
    *p++ = 0x80;
    *p++ = 0x52;

    /* blr x15 */
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x3F;
    *p++ = 0xD6;

    return CHUNK;
}

size_t print_reg(uint8_t *p, unsigned c)
{
    /* mov w0, wC */
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = BR + c;
    *p++ = 0x2A;

    /* Move the print register opcode into w1 */
    /* mov w1, OP_OUT */
    *p++ = 0x00 | (OP_OUT << 5) | OP_REG;
    *p++ = 0x00;
    *p++ = 0x80;
    *p++ = 0x52;

    /* blr x15 */
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x3F;
    *p++ = 0xD6;

    return CHUNK;
}

size_t read_into_reg(uint8_t *p, unsigned c)
{
    /* Move the read register opcode into w1 */
    /* mov w1, OP_IN */
    *p++ = 0x00 | (OP_IN << 5) | OP_REG;
    *p++ = 0x00;
    *p++ = 0x80;
    *p++ = 0x52;

    /* blr x15 */
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x3F;
    *p++ = 0xD6;

    /* Move result into register c
     * mov wC, w0 */
    *p++ = 0xE0 + (BR + c);
    *p++ = 0x03;
    *p++ = 0x00;
    *p++ = 0x2A;

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
    void *new_zero = mmap(NULL, copy_size * MULT,
                          PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 
                          -1, 0);
    memset(new_zero, 0, copy_size * MULT);

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

size_t inject_load_program(uint8_t *p, unsigned b, unsigned c)
{
    /* Move the 32-bit program counter into x10
     * mov x10, wC */
    *p++ = 0xEA;
    *p++ = 0x03;
    *p++ = 0x00 + (BR + c);
    *p++ = 0x2A;

    /* mov w0, wB */
    *p++ = 0xE0;
    *p++ = 0x03;
    *p++ = 0x00 + (BR + b);
    *p++ = 0x2A;

    /* br x15 */
    *p++ = 0xE0;
    *p++ = 0x01;
    *p++ = 0x1F;
    *p++ = 0xD6;

    return CHUNK;
}
