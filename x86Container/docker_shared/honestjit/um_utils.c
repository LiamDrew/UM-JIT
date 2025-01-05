#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "um_utils.h"
#include "arpa/inet.h"
#include <string.h>
#include <sys/mman.h>

unsigned char read_char(void)
{
    int x = getc(stdin);
    assert(x != EOF);
    unsigned char c = (unsigned char)x;
    // printf("X is %u\n", x);
    // printf("c is this %c\n", c);
    // print_regis
    return c;
}

void print_registers()
{
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    asm volatile(
        "movq %%r8, %0\n\t"
        "movq %%r9, %1\n\t"
        "movq %%r10, %2\n\t"
        "movq %%r11, %3\n\t"
        "movq %%r12, %4\n\t"
        "movq %%r13, %5\n\t"
        "movq %%r14, %6\n\t"
        "movq %%r15, %7\n\t"
        : "=m"(r8), "=m"(r9), "=m"(r10), "=m"(r11),
          "=m"(r12), "=m"(r13), "=m"(r14), "=m"(r15)
        :
        :);

    printf("R8: %lu\nR9: %lu\nR10: %lu\nR11: %lu\n"
           "R12: %lu\nR13: %lu\nR14: %lu\nR15: %lu\n",
           r8, r9, r10, r11, r12, r13, r14, r15);
}

uint32_t map_segment(uint32_t size)
{
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (gs.rec_size == 0) {
        /* Expand if necessary */
        if (gs.seq_size == gs.seq_cap) {
            gs.seq_cap *= 2;

            // realloc the array that keeps track of sequence size
            gs.seg_lens = realloc(gs.seg_lens, gs.seq_cap * sizeof(uint32_t));
            assert(gs.seg_lens != NULL);

            // also need to init the memory segment
            gs.val_seq = realloc(gs.val_seq, gs.seq_cap * sizeof(uint32_t*));
            assert(gs.val_seq != NULL);

            // Initializing all reallocated memory
            for (uint32_t i = gs.seq_size; i < gs.seq_cap; i++) {
                gs.val_seq[i] = NULL;
                gs.seg_lens[i] = 0;
            }
        }

        new_seg_id = gs.seq_size++;
    }

    /* Otherwise, reuse an old one */
    else {
        new_seg_id = gs.rec_ids[--gs.rec_size];
    }

    /* If the segment didn't previously exist or wasn't large enought for us*/
    if (gs.val_seq[new_seg_id] == NULL || size > gs.seg_lens[new_seg_id]) {

        gs.val_seq[new_seg_id] = realloc(gs.val_seq[new_seg_id], size * sizeof(uint32_t*));
        assert(gs.val_seq[new_seg_id] != NULL);
        gs.seg_lens[new_seg_id] = size;
    }

    /* zero out the segment */
    memset(gs.val_seq[new_seg_id], 0, size * sizeof(uint32_t));

    return new_seg_id;
}

void unmap_segment(uint32_t segmentId)
{
    if (gs.rec_size == gs.rec_cap) {
        gs.rec_cap *= 2;
        gs.rec_ids = realloc(gs.rec_ids, gs.rec_cap * sizeof(uint32_t));
    }

    gs.rec_ids[gs.rec_size++] = segmentId;
}

uint32_t segmented_load(uint32_t b_val, uint32_t c_val)
{
    uint32_t x = gs.val_seq[b_val][c_val];
    return x;
}

void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val)
{
    /* Load the inputted word into value memory */
    gs.val_seq[a_val][b_val] = c_val;

    /* NOTE: This only needs to happen if we are storing in the zero segment */
    // doing a segmented_store into the zero segment, so we have to compile
    // if (a_val == 0) {
    //     // what if I just don't do this?
    //     // NOTE: Some devious stuff is going on here
    //     // compile_instruction(gs.active, c_val, b_val * CHUNK);
    // }

}

void *load_program(uint32_t b_val, uint32_t c_val)
{
    /* Set the program counter to be the contents of register c */
    gs.pc = c_val;

    /* If segment zero is loaded, just return the active segment */
    if (b_val == 0) {
        return gs.active;
    }

    /* If a different segment is loaded, put that in */
    // TODO: Free the existing zero segment

    // if zero segment memory is large enough, reuse it

    // else, free it and realloc
    if (b_val > gs.seq_size) {
        printf("B val is %u\n", b_val);
        printf("Seq size is %u\n", gs.seq_size);
    }

    assert(b_val < gs.seq_size);
    
    uint32_t new_seg_size = gs.seg_lens[b_val];
    uint32_t *new_vals = calloc(new_seg_size, sizeof(uint32_t));
    memcpy(new_vals, gs.val_seq[b_val], new_seg_size * sizeof(uint32_t));

    /* Update the existing memory segment */
    gs.val_seq[0] = new_vals;
    gs.seg_lens[0] = new_seg_size;

    // this function will have to do the compiling for the new memory segment
    void *new_zero = mmap(NULL, new_seg_size * CHUNK,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(new_zero, 0, new_seg_size * CHUNK);

    uint32_t offset = 0;
    for (uint32_t i = 0; i < new_seg_size; i++) {
        offset = compile_instruction(new_zero, new_vals[i], offset);
    }

    gs.active = new_zero;
    return new_zero;
}

size_t compile_instruction(void *zero, Instruction word, size_t offset)
{
    uint32_t opcode = (word >> 28) & 0xF;
    uint32_t a = 0;

    // Now based on the opcode, figure out what to do

    /* Load Value */
    if (opcode == 13)
    {
        // printf("Load value a: %u, b: %u, c: %u\n", a, b, c);
        // Load the right register and do the thing
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
        // Load the rigther register and do the thing
        // printf("Output a: %u, b: %u, c: %u\n", a, b, c);
        offset += print_reg(zero, offset, c);
    }

    /* Addition */
    else if (opcode == 3)
    {
        // Load the right registers and do the thing
        // printf("Addition a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a, b, c);
    }

    /* Halt */
    else if (opcode == 7)
    {
        // printf("Haslt a: %u, b: %u, c: %u\n", a, b, c);
        offset += handle_halt(zero, offset);
    }

    /* Bitwise NAND */
    else if (opcode == 6)
    {
        // printf("Bitwise NAND a: %u, b: %u, c: %u\n", a, b, c);
        offset += nand_regs(zero, offset, a, b, c);
    }

    /* Addition */
    else if (opcode == 3)
    {
        // printf("Addtion a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a, b, c);
    }

    /* Multiplication */
    else if (opcode == 4)
    {
        // printf("Multiplication a: %u, b: %u, c: %u\n", a, b, c);
        offset += mult_regs(zero, offset, a, b, c);
    }

    /* Division */
    else if (opcode == 5)
    {
        // printf("Division a: %u, b: %u, c: %u\n", a, b, c);
        offset += div_regs(zero, offset, a, b, c);
    }

    /* Conditional Move */
    else if (opcode == 0)
    {
        // printf("Conditional move a: %u, b: %u, c: %u\n", a, b, c);
        offset += cond_move(zero, offset, a, b, c);
    }

    /* Input */
    else if (opcode == 11)
    {
        // printf("Input a: %u, b: %u, c: %u\n", a, b, c);
        offset += read_into_reg(zero, offset, c);
    }

    /* Segmented Load */
    else if (opcode == 1)
    {
        // printf("Segmented load a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_seg_load(zero, offset, a, b, c);
    }

    /* Segmented Store */
    else if (opcode == 2)
    {
        // printf("Segmented store a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_seg_store(zero, offset, a, b, c);
    }

    /* Load Program */
    else if (opcode == 12)
    {
        // printf("Load progam a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_load_program(zero, offset, b, c);
    }

    /* Map Segment */
    else if (opcode == 8)
    {
        // printf("Map segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_map_segment(zero, offset, b, c);
    }

    /* Unmap Segment */
    else if (opcode == 9)
    {
        // printf("Unmap segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_unmap_segment(zero, offset, c);
    }

    /* Invalid Opcode*/
    else
    {
        /* This value is not an instruction that is meant to be executed */
        /* Nothing is being written, but we still need a valid offset */
        offset += CHUNK;
    }

    return offset;
}

size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value)
{
    unsigned char *p = zero + offset;

    /* mov rXd, imm32 (where X is reg_num) */
    *p++ = 0x41;           // Reg prefix for r8-r15
    *p++ = 0xc7;           // mov immediate value to 32-bit register
    *p++ = 0xc0 | a; // ModR/M byte for target register

    *p++ = value & 0xFF;
    *p++ = (value >> 8) & 0xFF;
    *p++ = (value >> 16) & 0xFF;
    *p++ = (value >> 24) & 0xFF;

    // Jump forward 31 bytes
    *p++ = 0xEB;
    *p++ = 0x1F;

    return CHUNK;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;

    // move first source to eax
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc0 | (b << 3);

    // add second source to eax
    *p++ = 0x44;
    *p++ = 0x01;
    *p++ = 0xc0 | (c << 3);

    // move eax back to Rad
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | a;

    // jump 29 bytes
    *p++ = 0xEB;
    *p++ = 0x1D;

    return CHUNK;
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

    // 36 No Ops (but we return first so it's no biggie)

    return CHUNK;
}

size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;

    // mov eax, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    // mul rCd
    *p++ = 0x41;
    *p++ = 0xF7;
    *p++ = 0xE0 | c;

    // mov rAd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    // jump 29 bytes
    *p++ = 0xEB;
    *p++ = 0x1D;

    return CHUNK;
}

size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;

    *p++ = 0x48; // REX.W prefix for 64-bit operation
    *p++ = 0x31; // XOR r/m64, r64
    *p++ = 0xD2; // ModR/M: mod=11, reg=010 (rdx), r/m=010 (rdx)

    // put the dividend (reg b) in eax
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3);

    // div rC
    *p++ = 0x49;
    *p++ = 0xF7;
    *p++ = 0xF0 | c;

    // mov rA, rax
    *p++ = 0x49;
    *p++ = 0x89;
    *p++ = 0xC0 | a;

    // jump 26 bytes
    *p++ = 0xEB;
    *p++ = 0x1A;

    return CHUNK;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;

    // if rC != 0, Ra = Rb
    // cmp rC, 0
    *p++ = 0x41;           // REX.B
    *p++ = 0x83;           // CMP r/m32, imm8
    *p++ = 0xF8 | c; // ModR/M for CMP
    *p++ = 0x00;           // immediate 0

    // jz skip (over 3 bytes)
    *p++ = 0x74;
    *p++ = 0x03;

    // Note: I think this comment is wrong
    *p++ = 0x45;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3) | a;

    // jump 29 bytes
    *p++ = 0xEB;
    *p++ = 0x1D;

    return CHUNK;
}

size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;

    // move b to rax
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc0 | (b << 3);

    // and rcd with rax
    // and c with rax
    *p++ = 0x44;
    *p++ = 0x21;
    *p++ = 0xc0 | (c << 3);

    // not the whole thing
    *p++ = 0x40;
    *p++ = 0xf7;
    *p++ = 0xd0;

    // move to rad
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | a;

    // jump 26 bytes
    *p++ = 0xEB;
    *p++ = 0x1A;

    return CHUNK;
}

void print_out(uint32_t x)
{
    /* Registers MUST be saved in the inline assembly */
    unsigned char c = (unsigned char)x;
    putchar(c);
}

size_t print_reg(void *zero, size_t offset, unsigned c)
{
    // void *putchar_addr = (void *)&print_out;
    void *putchar_addr = (void *)&putchar;

    unsigned char *p = zero + offset;

    // mov edi, rXd (where X is reg_num)
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3); // ModR/M byte: edi(111) with reg number

    // push r8 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    /* This calling method has been a nightmare*/
    // int32_t call_offset = (int32_t)((uint64_t)putchar_addr - ((uint64_t)p + 5));
    // *p++ = 0xe8;
    // memcpy(p, &call_offset, sizeof(call_offset));
    // p += sizeof(call_offset);

    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &putchar_addr, sizeof(putchar_addr));
    p += sizeof(putchar_addr);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0; // ModR/M byte for call rax

    // pop r8 off the stack
    *p++ = 0x41;
    *p++ = 0x58;

    // jump 19 bytes
    *p++ = 0xEB;
    *p++ = 0x13;

    return CHUNK;
}

size_t read_into_reg(void *zero, size_t offset, unsigned c)
{
    unsigned char *p = zero + offset;

    void *read_char_addr = (void *)&read_char;

    // direct function call
    *p++ = 0x48;
    *p++ = 0xb8;

    memcpy(p, &read_char_addr, sizeof(read_char_addr));
    p += sizeof(read_char_addr);

    *p++ = 0xff;
    *p++ = 0xd0;

    // mov rCd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | c;

    *p++ = 0xEB;

    // Jump 23 bytes
    *p++ = 0x17;

    return CHUNK;
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    void *map_segment_addr = (void *)&map_segment;

    unsigned char *p = zero + offset;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg
    *p++ = 0xc7 | (c << 3); // ModR/M byte

    // push r8 - r11 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    *p++ = 0x41;
    *p++ = 0x51;

    *p++ = 0x41;
    *p++ = 0x52;

    *p++ = 0x41;
    *p++ = 0x53;


    // int32_t rel_offset = (int32_t)((uint64_t)map_segment_addr - ((uint64_t)p + 5));
    // // call rel32
    // *p++ = 0xE8; // Direct relative call
    // memcpy(p, &rel_offset, sizeof(rel_offset));
    // p += sizeof(rel_offset);

    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &map_segment_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0; // ModR/M byte for call rax

    // pop r8 - r11 off the stack
    *p++ = 0x41;
    *p++ = 0x5B;

    *p++ = 0x41;
    *p++ = 0x5A;

    *p++ = 0x41;
    *p++ = 0x59;

    *p++ = 0x41;
    *p++ = 0x58;

    // store the result in register b
    // move return value from rax to reg b
    // mov rB, rax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | b;

    // 40 - 34 = 6 No Ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    return CHUNK;
}

// size_t inject unmap segment
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    void *unmap_segment_addr = (void *)&unmap_segment;

    unsigned char *p = zero + offset;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg
    *p++ = 0xc7 | (c << 3); // ModR/M byte

    // push r8 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &unmap_segment_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0; // ModR/M byte for call rax

    // pop r8 off the stack
    *p++ = 0x41;
    *p++ = 0x58;

    // jump 19 bytes
    *p++ = 0xEB;
    *p++ = 0x13;

    return CHUNK;
}

// inject segmented load
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    void *seg_load_addr = (void *)&segmented_load;

    unsigned char *p = zero + offset;

    // mov rsi, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    // mov rdi, rCd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (c << 3);

    // push r8 - r11 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    *p++ = 0x41;
    *p++ = 0x51;

    *p++ = 0x41;
    *p++ = 0x52;

    *p++ = 0x41;
    *p++ = 0x53;

    // int32_t rel_offset = (int32_t)((uint64_t)seg_load_addr - ((uint64_t)p + 5));
    // *p++ = 0xe8;
    // memcpy(p, &rel_offset, sizeof(rel_offset));
    // p += sizeof(rel_offset);

    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &seg_load_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0; // ModR/M byte for call rax

    // pop r8 - r11 off the stack
    *p++ = 0x41;
    *p++ = 0x5B;

    *p++ = 0x41;
    *p++ = 0x5A;

    *p++ = 0x41;
    *p++ = 0x59;

    *p++ = 0x41;
    *p++ = 0x58;

    // return into correct register
    // move return value from rax to reg q
    // mov ra, rax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | a;

    // 40 - 37 = 3 No Ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    return CHUNK;
}

// TODO: having a lot of problems here
// // inject segmented load
// size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
// {
//     // void *seg_load_addr = (void *)&segmented_load;
//     void *val_seq_addr = (void *)&gs.val_seq;

//     unsigned char *p = zero + offset;

//     // load address of val_seq into RAX
//     *p++ = 0x48;
//     *p++ = 0xb8;
//     memcpy(p, &val_seq_addr, sizeof(void*));
//     p += sizeof(void*);

//     // mov rdi, rBd
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc7 | (b << 3);

//     // mov rsi, rCd
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc6 | (c << 3);

//     // mov rax, [rax + rdi*8]  (using rdi now for *8)
//     *p++ = 0x48;
//     *p++ = 0x8b;
//     *p++ = 0x04;
//     *p++ = 0xf8;

//     // mov eax, [rax + rsi*4]  (using rsi now for *4)
//     *p++ = 0x8b;
//     *p++ = 0x04;
//     *p++ = 0xb0;

//     // mov to return register
//     *p++ = 0x41;
//     *p++ = 0x89;
//     *p++ = 0xc0 | a;

//     // 12 No Ops
//     *p++ = 0xEB;
//     *p++ = 0x0C;

//     return CHUNK;
// }

// inject segmented store
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    void *val_seq_addr = (void *)&gs.val_seq;
    unsigned char *p = zero + offset;

    // Load gs.val_seq address into rax
    *p++ = 0x48; // REX.W
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &val_seq_addr, sizeof(void *));
    p += sizeof(void *);

    // mov rdi, rad
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (a << 3);

    // mov rsi, rbd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (b << 3);

    // mov rcx, rcd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc2 | (c << 3);

    // mov rax, [rax + rdi*8]
    *p++ = 0x48;
    *p++ = 0x8b;
    *p++ = 0x04;
    *p++ = 0xf8;

    // mov [rax + rsi*4], edx
    *p++ = 0x89;
    *p++ = 0x14;
    *p++ = 0xb0;

    // 40 - 28 bytes = jump 12 bytes
    *p++ = 0xEB;
    *p++ = 0x0C;

    return CHUNK;
}

// // inject segmented store
// size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
// {   
//     void *seg_store_addr = (void *)&segmented_store;

//     unsigned char *p = zero + offset;

//     // mov rsi, rad
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc7 | (a << 3);

//     // mov rdx, rbd
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc6 | (b << 3);

//     // mov rcx, rcd
//     *p++ = 0x44;
//     *p++ = 0x89;
//     *p++ = 0xc2 | (c << 3);

//     // push r8 - r11 onto the stack
//     *p++ = 0x41;
//     *p++ = 0x50;

//     *p++ = 0x41;
//     *p++ = 0x51;

//     *p++ = 0x41;
//     *p++ = 0x52;

//     *p++ = 0x41;
//     *p++ = 0x53;

//     // int32_t rel_offset = (int32_t)((uint64_t)seg_store_addr - ((uint64_t)p + 5));
//     // *p++ = 0xe8;
//     // memcpy(p, &rel_offset, sizeof(rel_offset));
//     // p += sizeof(rel_offset);

//     *p++ = 0x48; // REX.W prefix
//     *p++ = 0xb8; // mov rax, imm64
//     memcpy(p, &seg_store_addr, sizeof(void*));
//     p += sizeof(void*);

//     // call rax
//     *p++ = 0xff;
//     *p++ = 0xd0; // ModR/M byte for call rax

//     // pop r8 - r11 off the stack
//     *p++ = 0x41;
//     *p++ = 0x5B;

//     *p++ = 0x41;
//     *p++ = 0x5A;

//     *p++ = 0x41;
//     *p++ = 0x59;

//     *p++ = 0x41;
//     *p++ = 0x58;

//     // 40 - 37 = 3 no ops
//     *p++ = 0x0F;
//     *p++ = 0x1F;
//     *p++ = 0x00;

//     return CHUNK;
// }

// inject load program
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    void *load_program_addr = (void *)&load_program;

    unsigned char *p = zero + offset;

    // stash b in the right register (even if 0, need to update program pointer)
    // move b to rdi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    // stash c val in the right register
    // move c to rsi
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (c << 3);

    // push r8 - r11 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    *p++ = 0x41;
    *p++ = 0x51;

    *p++ = 0x41;
    *p++ = 0x52;

    *p++ = 0x41;
    *p++ = 0x53;

    *p++ = 0x48;
    *p++ = 0xb8;
    memcpy(p, &load_program_addr, sizeof(load_program_addr));
    p += sizeof(load_program_addr);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0;
    
    // this function better return rax as the right thing
    // injected function needs to ret (rax should already be the right thing)

    // pop r8 - r11 off the stack
    *p++ = 0x41;
    *p++ = 0x5B;

    *p++ = 0x41;
    *p++ = 0x5A;

    *p++ = 0x41;
    *p++ = 0x59;

    *p++ = 0x41;
    *p++ = 0x58;

    // 40 - 35 = 5 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;

    *p++ = 0x90;
    *p++ = 0x90;

    // ret
    *p++ = 0xc3;

    return CHUNK;
}


// NOTEs for readme:
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

/* It's important to remember that this code doesn't get executed as it is
 * written. It was tempting to optimize this by moving the register that
 * [value] is stored in into the target register, but I had to keep in mind
 * that when this code gets executed, [value] will no longer be in the
 * target register. The safest move is to hardcode the 32-bit value in
 * little endian order to be loaded into the register.
 */

/* ModR/M byte Format:
 * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */