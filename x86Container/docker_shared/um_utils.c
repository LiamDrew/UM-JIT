#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "um_utils.h"
#include "arpa/inet.h"
#include <string.h>

unsigned char read_char(void)
{
    printf("Reading in char\n");
    printf("Global state seq size: %d\n", gs.seq_size);
    int x = getc(stdin);
    assert(x != EOF);
    return (unsigned char)x;
}

uint32_t map_segment(uint32_t size)
{
    printf("Mapping segment\n");
    printf("Raw value is %u (0x%x)\n", size, size);

    // Size will be stored in register c
    // it will be passed in as an argument to the function

    // mmap something

    // return the address of the mmaped memory
    // return;

    // the injected assembly will store the address in register b
    // Store memory address in register b
    // assert(false);

    return 8008135;
}

// void unmap segment(void *segmentId)
void unmap_segment(uint32_t segmentId)
{
    printf("Unmapping segment\n");
    printf("Seg id is: %u\n", segmentId);
    // TODO: do the unmapping
}

// segmented load
uint32_t segmented_load(uint32_t b_val, uint32_t c_val)
{
    printf("Segmented load\n");
    printf("Reg b is %u\n", b_val);
    printf("Reg c is %u\n", c_val);

    // get the segment we care about 

    // In this case, we strictly get the value, since it would make no sense to
    // load a bunch of assembly instruction into a register

    // this gets loaded into a register on the assembly side
    return gs.val_seq[b_val][c_val];
}

// segmented store (will have to compile r[C] to machine code inline)
/*
 * Since this function may have to compile assembly inline, it may have to be
 * called by 8 byte function pointer instead of 4 byte offset (if it wants to live in main
 * and have access to all the functions that can access the other functions)
 * 
 * Alternatively, everything could get moved into this utils file except the main function.
 */
void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val)
{
    printf("Segmented store\n");
    printf("Reg a is %u\n", a_val);
    printf("Reg b is %u\n", b_val);
    printf("Reg c is %u\n", c_val);

    // TODO: unpack the word into the stuff

    // In this case, we have to both compile to machine code and store the native UM word
    // we have compile instruction available to us. We just need to give it a memory segment and an offset.

    // and of course it will be easy to load something straight into memory
    gs.val_seq[a_val][b_val] = c_val;

    // compile step
    compile_instruction(gs.program_seq[a_val], c_val, b_val);

    return;
}

// load program
/* Load program needs to do something rather important:
 * update the memory address of the new segment being executed
 */
void *load_program(uint32_t b_val, uint32_t c_val)
{
    printf("Load program\n");
    printf("Reg b is %u\n", b_val);
    printf("Reg c is %u\n", c_val);

    // set program counter to the contents of register c
    // gs.pc = c_val;
    gs.pc = 4;

    // if (b_val == 0) {
    //     // return the address of the 0 segment
    //     return gs.program_seq[0];
    // }

    return gs.program_seq[b_val];

    // return the address of the right memory segment
    // return NULL;
}

size_t compile_instruction(void *zero, Instruction word, size_t offset)
{
    uint32_t opcode = (word >> 28) & 0xF;

    // printf("Opcode: %d\n", opcode);
    uint32_t a = 0, b = 0, c = 0, val = 0;

    if (opcode == 13)
    {
        a = (word >> 25) & 0x7;
        val = word & 0x1FFFFFF;
        // printf("Reg a is %d\n", a);
        // printf("Val to load is %d\n", val);
    }
    
    else
    {
        c = word & 0x7;
        b = (word >> 3) & 0x7;
        a = (word >> 6) & 0x7;
        // printf("Reg a is %d\n", a);
        // printf("Reg b is %d\n", b);
        // printf("Reg c is %d\n", c);
    }

    // Now based on the opcode, figure out what to do

    /* Load Value */
    if (opcode == 13)
    {
        // Load the right register and do the thing
        offset += load_reg(zero, offset, a + RO, val);
    }

    /* Output */
    else if (opcode == 10)
    {
        // Load the rigther register and do the thing
        printf("Output a: %u, b: %u, c: %u\n", a, b, c);
        offset += print_reg(zero, offset, c + RO);
    }

    /* Addition */
    else if (opcode == 3)
    {
        // Load the right registers and do the thing
        printf("Addition a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Halt */
    else if (opcode == 7)
    {
        printf("Halt a: %u, b: %u, c: %u\n", a, b, c);
        offset += handle_halt(zero, offset);
    }

    /* Bitwise NAND */
    else if (opcode == 6)
    {
        printf("Bitwise NAND a: %u, b: %u, c: %u\n", a, b, c);
        offset += nand_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Addition */
    else if (opcode == 3)
    {
        offset += add_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Multiplication */
    else if (opcode == 4)
    {
        printf("Multiplication a: %u, b: %u, c: %u\n", a, b, c);
        offset += mult_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Division */
    else if (opcode == 5)
    {
        printf("Division a: %u, b: %u, c: %u\n", a, b, c);
        offset += div_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Conditional Move */
    else if (opcode == 0)
    {
        printf("Conditional move a: %u, b: %u, c: %u\n", a, b, c);
        offset += cond_move(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Input */
    else if (opcode == 11)
    {
        printf("Input a: %u, b: %u, c: %u\n", a, b, c);
        offset += read_into_reg(zero, offset, c + RO);
    }

    /* Segmented Load */
    else if (opcode == 1)
    {
        printf("Segmented load a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_seg_load(zero, offset, a + RO, b + RO, c + RO, word);
    }

    /* Segmented Store */
    else if (opcode == 2)
    {
        printf("Segmented store a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_seg_store(zero, offset, a + RO, b + RO, c + RO, word);
    }

    /* Load Program */
    else if (opcode == 12)
    {
        printf("Load progam a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_load_program(zero, offset, b + RO, c + RO);
    }

    /* Map Segment */
    else if (opcode == 8)
    {
        printf("Map segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_map_segment(zero, offset, b + RO, c + RO);
    }

    /* Unmap Segment */
    else if (opcode == 9)
    {
        printf("Unmap segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_unmap_segment(zero, offset, c + RO);
    }

    /* Invalid Opcode*/
    else
    {
        printf("Opcode: %d\n", opcode);
        assert(false);
    }

    return offset;
}

size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value)
{
    // Remove this check when JIT is running
    if (a < 8 || a > 15)
        assert(false);

    unsigned char *p = zero + offset;

    /* To avoid a jump table to determine what register to load the value into,
     * I'm going to use a dirty little trick to load value into the correct
     * register.
     * I'm Bitwise ORing the machine code for the mov instruction with the
     * register number to load the register I want.
     * This is only possible by breaking the machine code abstraction and
     * is not possible in assembly (that I know of).
     * Here, the contents of the machine code are determined at runtime and not
     * compile time
     */

    /* mov rXd, imm32 (where X is reg_num) */
    *p++ = 0x41;           // Reg prefix for r8-r15
    *p++ = 0xc7;           // mov immediate value to 32-bit register
    *p++ = 0xc0 | (a - 8); // ModR/M byte for target register

    /* It's important to remember that this code doesn't get executed as it is
     * written. It was tempting to optimize this by moving the register that
     * [value] is stored in into the target register, but I had to keep in mind
     * that when this code gets executed, [value] will no longer be in the
     * target register. The safest move is to hardcode the 32-bit value in
     * little endian order to be loaded into the register.
     */

    *p++ = value & 0xFF;
    *p++ = (value >> 8) & 0xFF;
    *p++ = (value >> 16) & 0xFF;
    *p++ = (value >> 24) & 0xFF;

    // 9 NoOps to align with chunk boundary
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
    //return 7;
}

void print_out(uint32_t x)
{
    printf("x is %u\n", x);
    unsigned char c = (unsigned char)x;
    printf("Unsigned char is %c\n", c);

    putchar(c);
}

size_t print_reg(void *zero, size_t offset, unsigned reg)
{
    // Remove this check when JIT is running
    if (reg < 8 || reg > 15)
        assert(false);

    // void *putchar_addr = (void *)&putchar;
    void *putchar_addr = (void *)&print_out;
    

    unsigned char *p = zero + offset;

    // mov edi, rXd (where X is reg_num)
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = (0xc7 | ((reg - 8) << 3)); // ModR/M byte: edi(111) with reg number

    // call putchar
    *p = 0xe8;
    p++;

    int32_t call_offset = (int32_t)((unsigned char *)putchar_addr - (p + 4));
    memcpy(p, &call_offset, sizeof(int32_t));
    p += sizeof(int32_t);

    /* 3 to load reg into edi, 1 for call instruction, 4 for putchar addr */

    // 8 NoOPs to align with chunk boundary
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
    // return 8;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    // Remove this check when JIT is running
    // printf("a: %u, b: %u, c: %u\n", a, b, c);
    if (a < 8 || a > 15)
        assert(false);
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    // TODO: verify that this works correctly for large numbers

    unsigned char *p = zero + offset;

    // mov rAd, rBd (move b to a)
    *p++ = 0x45; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = 0xc0 | ((b - 8) << 3) | (a - 8); // ModR/M byte

    // add rAd, rCd (add c to a)
    *p++ = 0x45;                            // Reg prefix for r8-r15
    *p++ = 0x01;                            // add reg to reg
    *p++ = 0xc0 | ((c - 8) << 3) | (a - 8); // ModR/M byte

    // 10 NoOps
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
    // return 6;
}

size_t handle_halt(void *zero, size_t offset)
{
    unsigned char *p = zero + offset;

    // set RAX to 0 (NULL);
    // xor rax,rax
    *p++ = 0x48;
    *p++ = 0x31;
    *p++ = 0xc0;

    // ret
    *p++ = 0xc3;

    // 12 NoOps
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
    // Remove this check when JIT is running
    if (a < 8 || a > 15)
        assert(false);
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    // TODO: verify this works correctly with large numbers
    unsigned char *p = zero + offset;

    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | ((b - 8) << 3);

    // mul rCd
    *p++ = 0x41;
    *p++ = 0xF7;
    *p++ = 0xE0 | (c - 8);

    // mov rAd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | (a - 8);

    // 7 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;

    return CHUNK;

    // return 9;
}

size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    // Remove this check when JIT is running
    if (a < 8 || a > 15)
        assert(false);
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    unsigned char *p = zero + offset;

    *p++ = 0x48; // REX.W prefix for 64-bit operation
    *p++ = 0x31; // XOR r/m64, r64
    *p++ = 0xD2; // ModR/M: mod=11, reg=010 (rdx), r/m=010 (rdx)

    // put the dividend (reg b) in eax
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | ((b - 8) << 3);

    // div rC
    *p++ = 0x49;
    *p++ = 0xF7;
    *p++ = 0xF0 | (c - 8);

    // mov rA, rax
    *p++ = 0x49;
    *p++ = 0x89;
    *p++ = 0xC0 | (a - 8);

    // 4 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;

    return CHUNK;
    // return 12;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    // Remove this check when JIT is running
    if (a < 8 || a > 15)
        assert(false);
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    unsigned char *p = zero + offset;

    // if rC != 0, Ra = Rb

    // cmp rC, 0
    *p++ = 0x41;           // REX.B
    *p++ = 0x83;           // CMP r/m32, imm8
    *p++ = 0xF8 | (c - 8); // ModR/M for CMP
    *p++ = 0x00;           // immediate 0

    // jz skip (over 3 bytes)
    *p++ = 0x74;
    *p++ = 0x03;

    // mov rAd, rAd
    *p++ = 0x45;
    *p++ = 0x89;
    *p++ = 0xC0 | ((b - 8) << 3) | (a - 8);

    // 7 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;

    return CHUNK;
    // return 9;
}

size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    // Remove this check when JIT is running
    if (a < 8 || a > 15)
        assert(false);
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    unsigned char *p = zero + offset;

    // mov rAd, rBd (move b to a)
    *p++ = 0x45;
    *p++ = 0x89;
    *p++ = 0xc0 | ((b - 8) << 3) | (a - 8);

    // and rAd, rCd (and c to a)
    *p++ = 0x45;
    *p++ = 0x21;
    *p++ = 0xc0 | ((c - 8) << 3) | (a - 8);

    // not rAd (not a)
    *p++ = 0x41;
    *p++ = 0xf7;
    *p++ = 0xd0 | (a - 8);

    // 7 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;

    return CHUNK;
    // return 9;
}

size_t read_into_reg(void *zero, size_t offset, unsigned reg)
{
    // Remove this check when JIT is running
    if (reg < 8 || reg > 15)
        assert(false);

    unsigned char *p = zero + offset;

    void *read_char_addr = (void *)&read_char;

    // Since we're using PIC, let's use a direct relative call
    // This will be a 5-byte instruction: E8 + 32-bit offset
    int32_t rel_offset = (int32_t)((uint64_t)read_char_addr - ((uint64_t)p + 5));

    // call rel32
    *p++ = 0xE8; // Direct relative call
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // mov rCd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | (reg - 8);

    // 8 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
    // return 8;
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    void *map_segment_addr = (void *)&map_segment;

    unsigned char *p = zero + offset;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = 0xc7 | ((c - 8) << 3); // ModR/M byte

    int32_t rel_offset = (int32_t)((uint64_t)map_segment_addr - ((uint64_t)p + 5));

    // call rel32
    *p++ = 0xE8; // Direct relative call
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // store the result in register b
    // move return value from rax to reg b
    // mov rB, rax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | (b - 8);

    // 5 No Ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
    // return 11;
}

// size_t inject unmap segment
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    if (c < 8 || c > 15)
        assert(false);
    void *unmap_segment_addr = (void *)&unmap_segment;

    unsigned char *p = zero + offset;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = 0xc7 | ((c - 8) << 3); // ModR/M byte

    int32_t rel_offset = (int32_t)((uint64_t)unmap_segment_addr - ((uint64_t)p + 5));

    *p++ = 0xe8;
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // no return value from the unmap segment function

    // 8 No Ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
    // return 8;
}

// inject segmented load
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c, Instruction word)
{
    (void)word;
    if (a < 8 || a > 15)
        assert(false);
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    /* the instruction needs to be passed as an argument and unpacked inline
     * because it's too space expensive to do it in assembly
     * This choice will majorly throttle the compiler speed, we can revisit later */

    void *seg_load_addr = (void *)&segmented_load;

    unsigned char *p = zero + offset;

    // mov rsi, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | ((b - 8) << 3);

    // mov rdi, rCd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | ((c - 8) << 3);

    int32_t rel_offset = (int32_t)((uint64_t)seg_load_addr - ((uint64_t)p + 5));

    *p++ = 0xe8;
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // return into correct register
    // move return value from rax to reg q
    // mov ra, rax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | (a - 8);

    // 2 No Ops
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
    // return 14;
}

// inject segmented store
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c, Instruction word)
{
    (void)word;
    void *seg_store_addr = (void *)&segmented_store;

    unsigned char *p = zero + offset;

    // This takes way too many assembly instructions

    // /* mov rdi, imm32 (where X is reg_num) */
    // *p++ = 0x40; // Reg prefix for r8-r15
    // *p++ = 0xc7; // mov immediate value to 32-bit register
    // *p++ = 0xc7;

    // *p++ = word & 0xFF;
    // *p++ = (word >> 8) & 0xFF;
    // *p++ = (word >> 16) & 0xFF;
    // *p++ = (word >> 24) & 0xFF;

    // mov rsi, rad
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | ((a - 8) << 3);

    // mov rdx, rbd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | ((b - 8) << 3);

    // mov rcx, rcd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc2 | ((c - 8) << 3);

    int32_t rel_offset = (int32_t)((uint64_t)seg_store_addr - ((uint64_t)p + 5));

    *p++ = 0xe8;
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // 2 No Ops
    *p++ = 0x90;
    *p++ = 0x90;

    return CHUNK;
    // return 14;
}

// inject load program
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    if (b < 8 || b > 15)
        assert(false);
    if (c < 8 || c > 15)
        assert(false);

    void *load_program_addr = (void *)&load_program;

    unsigned char *p = zero + offset;

    // stash b in the right register (even if 0, need to update program pointer)
    // move b to rdi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | ((b - 8) << 3);

    // stash c val in the right register
    // move c to rsi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | ((c - 8) << 3);

    // call function
    int32_t rel_offset = (int32_t)((uint64_t)load_program_addr - ((uint64_t)p + 5));

    *p++ = 0xe8;
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // injected function needs to ret (rax should already be the right thing)

    // ret
    *p++ = 0xc3;

    // 4 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;

    return CHUNK;
    // return 12;
}

// have to compile the contents of c_val back into a UM instruction, and store in the right segment
// this is really tricky. Maybe this is just a number being saved that's going back into a register?
// or maybe it's secretely an instruction that's being encoded?
// or both? This is going to require serious thought, and could totally mess up the whole compile ahead
// of time plan. Idk what I'm going to do about this
/*
 * the problem is that anything in a segment is fair game to be executed as an instruction if the segment is loaded
 * The contents of the segment at that point could either be a value that's going to get loaded later, or
 * an instruction that's about to be executed. How can we differentiate between values to load and instructions?
 * You could concievably have a number that has a valid opcode and valid numbers for all registers, but that is not going to be executed as
 * an instruction and is just going to be loaded from memory at a future point.
 * Conversely, the very same number could be executed as a valid instruction. There is no way to know which is which. If you try to load an instruction that was compiled that shouldn't have been compiled, you're going to have major problems and things will make no sense.
 * Conversely, if you encounter an instruction that was not compiled that should have been compiled, your program will just segfault.
 * There's no way to tell until the instruction attempts to be executed.
 *
 * Potential workaround: You both compile and store. The compiled code goes in mmap segment, and the stored value goes in the calloc'ed segment.
 * However, this would be enourmously space inefficient and be somewhat time inefficient with poor spatial locality
 *
 * This might be the best option. I wonder if the 4 bits could be included in line with the instructions, but just be skipped over by the assembly.
 * That way, if the segment was referenced, there would be a clear protocol for what bits the actual information would be encoded in. But if
 * it came time for the actual instruction to be executed, the machine code would clearly indicate that the 4 bits would be garbage and not meant
 * for executing. This would be a really far out of the box solution, but is exactly the type of high speed hardware in the loop thinking we're
 * going for here. Good talk.
 *
 * Big problem: putting this inline with the assembly will bloat the instruction number enourmously. This is already a big concern because we have
 * to load 3 registers as function arguments, so adding another 6 assembly instruction will be enourmously space-expensive considering the current
 * plan to pad everything so that memory is still chunk-addressable.
 *
 * If i really needed to save assembly instructions, I could send the bitpacked word in as one argument instead of 3, and then unpack it inside     the function. That would suck though.
 *
 * We're already signed up for 5x bloat. Let's try it with the bitpack plan and see what can be done.
 */