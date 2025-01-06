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
    unsigned char c = (unsigned char)x;
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
    if (gs.rec_size == 0)
    {
        /* Expand if necessary */
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

    /* Otherwise, reuse an old one */
    else
    {
        new_seg_id = gs.rec_ids[--gs.rec_size];
    }

    /* If the segment didn't previously exist or wasn't large enought for us*/
    if (gs.val_seq[new_seg_id] == NULL || size > gs.seg_lens[new_seg_id])
    {
        gs.val_seq[new_seg_id] = realloc(gs.val_seq[new_seg_id], size * sizeof(uint32_t));
        assert(gs.val_seq[new_seg_id] != NULL); // Make sure the realloc didn't fail

        gs.seg_lens[new_seg_id] = size;
    }

    /* zero out the segment */
    memset(gs.val_seq[new_seg_id], 0, size * sizeof(uint32_t));

    return new_seg_id;
}

// void unmap_segment(uint32_t segmentId)
// {
//     if (gs.rec_size == gs.rec_cap)
//     {
//         gs.rec_cap *= 2;
//         gs.rec_ids = realloc(gs.rec_ids, gs.rec_cap * sizeof(uint32_t));
//     }

//     gs.rec_ids[gs.rec_size++] = segmentId;
// }

void unmap_segment(uint32_t segmentId)
{
    uintptr_t base_addr = (uintptr_t)&gs;

    uintptr_t rec_ids_addr = base_addr + 36;
    uintptr_t rec_size_addr = base_addr + 44;
    uintptr_t rec_cap_addr = base_addr + 48;

    // Access the values using the calculated addresses
    uint32_t rec_size = *(uint32_t *)rec_size_addr;
    uint32_t rec_cap = *(uint32_t *)rec_cap_addr;
    uint32_t *rec_ids = *(uint32_t **)rec_ids_addr;

    if (rec_size == rec_cap)
    {
        rec_cap *= 2;
        rec_ids = realloc(rec_ids, rec_cap * sizeof(uint32_t));

        *(uint32_t *)rec_cap_addr = rec_cap;
        *(uint32_t **)rec_ids_addr = rec_ids;
    }

    rec_ids[rec_size++] = segmentId;
    *(uint32_t *)rec_size_addr = rec_size;
}

uint32_t *help_unmap(uint32_t *rids, uint32_t bytes)
{
    // (void)rids;
    // (void)bytes;
    // printf("Recycled segment size is %u\n", gs.rec_size);
    // printf("Recycled segment capacity is %u\n", gs.rec_cap);
    // printf("About to realloc %u bytes\n", bytes);
    // printf("First pointer is %p\n", rids);
    uint32_t *temp = realloc(rids, bytes);
    assert(temp != NULL);
    // printf("After pointer is %p\n", temp);
    memset(temp, 0, bytes);
    gs.rec_ids = temp;
    // printf("Alloc successfull\n");
    return temp;
}

void checkpoint(uint32_t *in, uint32_t rec_id, uint32_t size)
{
    // (void)in;m
    in[size] = rec_id;
    printf("Inputted pointer is %p\n", in);
    printf("Segment ID getting recycled is %u\n", rec_id);
    printf("Size is %u\n", size);

    // printf("Recycled segment size is %u\n", gs.rec_size);
    // printf("Recycled segment capacity is %u\n", gs.rec_cap);
    // printf("pointer is %p\n", gs.rec_ids);
}

// uint32_t segmented_load(uint32_t b_val, uint32_t c_val)
// {
//     uint32_t x = gs.val_seq[b_val][c_val];
//     return x;
// }

// void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val)
// {
//     /* Load the inputted word into value memory */
//     gs.val_seq[a_val][b_val] = c_val;

//     /* NOTE: This only needs to happen if we are storing in the zero segment */
//     // doing a segmented_store into the zero segment, so we have to compile
//     // if (a_val == 0) {
//     //     // what if I just don't do this?
//     //     // NOTE: Some devious stuff is going on here
//     //     // compile_instruction(gs.active, c_val, b_val * CHUNK);
//     // }
// }

void *load_program(uint32_t b_val, uint32_t c_val)
{
    (void)c_val;
    // The following two steps get handled in inline assembly
    /* Set the program counter to be the contents of register c */
    /* If segment zero is loaded, just return the active segment */

    /* If a different segment is loaded, put that in */
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
    for (uint32_t i = 0; i < new_seg_size; i++)
    {
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
    unsigned char *s = p;

    /* mov rXd, imm32 (where X is reg_num) */
    *p++ = 0x41;     // Reg prefix for r8-r15
    *p++ = 0xc7;     // mov immediate value to 32-bit register
    *p++ = 0xc0 | a; // ModR/M byte for target register

    *p++ = value & 0xFF;
    *p++ = (value >> 8) & 0xFF;
    *p++ = (value >> 16) & 0xFF;
    *p++ = (value >> 24) & 0xFF;

    // Jump forward 31 bytes 
    // + 24 = 55
    // This should automatically jump forward the correct number of bytes
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));

    // printf("this chunk is %lu\n", (CHUNK - (p - s + 1)));

    return CHUNK;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

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
    // + 24 = 53
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));

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

    return CHUNK;
}

size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

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
    *p = 0x00 | (CHUNK - (p - s + 1));
    // *p++ = 0x1D;

    return CHUNK;
}

size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

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
    *p = 0x00 | (CHUNK - (p - s + 1));
    // *p++ = 0x1A;

    return CHUNK;
}

size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

    // if rC != 0, Ra = Rb
    // cmp rC, 0
    *p++ = 0x41;     // REX.B
    *p++ = 0x83;     // CMP r/m32, imm8
    *p++ = 0xF8 | c; // ModR/M for CMP
    *p++ = 0x00;     // immediate 0

    // jz skip (over 3 bytes)
    *p++ = 0x74;
    *p++ = 0x03;

    // Note: I think this comment is wrong
    *p++ = 0x45;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3) | a;

    // jump 29 bytes
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));
    // *p++ = 0x1D;

    return CHUNK;
}

size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

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
    *p = 0x00 | (CHUNK - (p - s + 1));
    // *p++ = 0x1A;

    return CHUNK;
}

size_t print_reg(void *zero, size_t offset, unsigned c)
{
    // void *putchar_addr = (void *)&print_out;
    void *putchar_addr = (void *)&putchar;

    unsigned char *p = zero + offset;
    unsigned char *s = p;

    // mov edi, rXd (where X is reg_num)
    *p++ = 0x44; // Reg prefix for r8-r15
    *p++ = 0x89;
    *p++ = 0xc7 | (c << 3); // ModR/M byte: edi(111) with reg number

    // push r8 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    *p++ = 0x41;
    *p++ = 0x51;

    *p++ = 0x41;
    *p++ = 0x52;

    *p++ = 0x41;
    *p++ = 0x53;

    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &putchar_addr, sizeof(putchar_addr));
    p += sizeof(putchar_addr);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0; // ModR/M byte for call rax

    // pop r8 off the stack
    *p++ = 0x41;
    *p++ = 0x5B;

    *p++ = 0x41;
    *p++ = 0x5A;

    *p++ = 0x41;
    *p++ = 0x59;

    *p++ = 0x41;
    *p++ = 0x58;

    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));

    return CHUNK;
}

size_t read_into_reg(void *zero, size_t offset, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

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
    *p = 0x00 | (CHUNK - (p - s + 1));

    // Jump 23 bytes
    // *p++ = 0x17;

    return CHUNK;
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    void *map_segment_addr = (void *)&map_segment;

    unsigned char *p = zero + offset;
    unsigned char *s = p;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44;            // Reg prefix for r8-r15
    *p++ = 0x89;            // mov reg to reg
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

    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));

    return CHUNK;
}

// OLD Version
// size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
// {
//     void *help_unmap_addr = (void *)&help_unmap;
//     unsigned char *p = zero + offset;
//     unsigned char *s = p;

//     // load the address of gs.rec_size into rbx
//     *p++ = 0x48;
//     *p++ = 0xbb;
//     uint64_t addr = (uint64_t)&gs.rec_size;
//     memcpy(p, &addr, sizeof(addr));
//     p += 8;

//     // load the value of rec size and rec cap into registers

//     // stick reg size in rdx
//     // dereference rbx and stick into rdx
//     *p++ = 0x48;
//     *p++ = 0x8b;
//     *p++ = 0x13;

//     // stick rec_cap in rsi
//     // dereference rbx + 4 and stick rsi 
//     // (this capacity will be the second argument to realloc, hence rsi)
//     *p++ = 0x48;
//     *p++ = 0x8b;
//     *p++ = 0x73;
//     *p++ = 0x04;

//     // compare them (no rex needed for 32bit comparison)
//     *p++ = 0x39;
//     *p++ = 0xf2; // 11 001 010

//     // // if not equal, jump a long way
//     *p++ = 0x75;
//     *p++ = 0x30; // TODO: update the jump distance
//     // normally 32 instead of 36
//     // *p++ = 0x36;

//     //_________________________________
//     // if not equal
//     // double rec cap (add it to itself)
//     // add rsi to itself
//     *p++ = 0x01;
//     *p++ = 0xF6; // 11 110 110

//     // store the new value of rsi back into memory
//     *p++ = 0x48;
//     *p++ = 0x89; //89 is for moving to memory, 8b is for moving from memory
//     *p++ = 0x73; // ModRM 01, 110, 011 (2 bits, source reg, base pointer reg)
//     *p++ = 0x04;

//     // move rec_ids into rdi
//     // mov rdi, [rbx - 8]
//     *p++ = 0x48; // REX.W prefix (since rec_ids is a pointer)
//     *p++ = 0x8b; // mov
//     *p++ = 0x7b; // ModR/M byte for rdi with displacement (01 111 000)
//     *p++ = 0xf8; // -8 in two's complement (0xf8 = -8)

//     // the doubled reg capacity is already in rsi. We're about to multiply it by 4 (for sizeof int)
//     // shl rsi, 2  (multiply by 4 = 2^2)
//     *p++ = 0x48; // REX.W prefix for 64-bit
//     *p++ = 0xc1; // shift with immediate byte
//     *p++ = 0xe6; // ModR/M: 11 100 110 (rsi)
//     *p++ = 0x02; // shift by 2 positions

//     // push rdx (rec_size) to the stack
//     *p++ = 0x52;

//     // push r8 onto the stack
//     *p++ = 0x41;
//     *p++ = 0x50;

//     *p++ = 0x41;
//     *p++ = 0x51;

//     *p++ = 0x41;
//     *p++ = 0x52;

//     *p++ = 0x41;
//     *p++ = 0x53;

//     // call realloc
//     *p++ = 0x48; // REX.W prefix
//     *p++ = 0xb8; // mov rax, imm64
//     // memcpy(p, &realloc, sizeof(void *));
//     // p += sizeof(void *);
//     memcpy(p, &help_unmap_addr, sizeof(void *));
//     p += sizeof(void *);

//     // call rax
//     *p++ = 0xff;
//     *p++ = 0xd0; // ModR/M byte for call rax

//     // pop r8 off the stack
//     *p++ = 0x41;
//     *p++ = 0x5B;

//     *p++ = 0x41;
//     *p++ = 0x5A;

//     *p++ = 0x41;
//     *p++ = 0x59;

//     *p++ = 0x41;
//     *p++ = 0x58;

//     *p++ = 0x5A; // pop rdx

//     // store the result (in rax) back in gs.rec_ids
//     *p++ = 0x48;
//     *p++ = 0x89;
//     *p++ = 0x43; // mod rm
//     *p++ = 0xf8; // - 8 offset

//     // // set RAX to 0 (NULL);
//     // *p++ = 0x48;
//     // *p++ = 0x31;
//     // *p++ = 0xc0;
//     // *p++ = 0xc3;

//     //__________________________

//     // JUMP LANDING POINT
//     // store segmentID in gs.rec_ids[gs.rec_size]

//     // First load rec_ids pointer from [rbx - 8] into rax
//     *p++ = 0x48; // REX.W prefix
//     *p++ = 0x8b; // mov
//     *p++ = 0x43; // ModRM: 01 000 011
//     *p++ = 0xf8; // -8 displacement

//     // First load rec_ids pointer from [rbx - 8] into rdi
//     *p++ = 0x48; // REX.W prefix
//     *p++ = 0x8b; // mov
//     *p++ = 0x7b; // ModRM: 01 111 011 (changed from 0x43 to 0x7b)
//     *p++ = 0xf8; // -8 displacement

//     // rC is the segment id getting recycled
//     // mov rsi, rC
//     *p++ = 0x44;            // Reg prefix for r8-r15
//     *p++ = 0x89;            // mov reg to reg
//     *p++ = 0xc6 | (c << 3); // ModR/M byte - changed from 0xc7 to 0xc6

//     // // rdx is gs.rec_size (don't need to change it)
//     // // push rdx (rec_size) to the stack
//     *p++ = 0x52;

//     // push r8 onto the stack
//     *p++ = 0x41;
//     *p++ = 0x50;

//     *p++ = 0x41;
//     *p++ = 0x51;

//     *p++ = 0x41;
//     *p++ = 0x52;

//     *p++ = 0x41;
//     *p++ = 0x53;

//     void *check_addr = (void *)&checkpoint;
//     // 12 byte function call
//     *p++ = 0x48; // REX.W prefix
//     *p++ = 0xb8; // mov rax, imm64
//     memcpy(p, &check_addr, sizeof(void *));
//     p += sizeof(void *);
//     *p++ = 0xff;
//     *p++ = 0xd0; // ModR/M byte for call rax

//     // pop r8 off the stack
//     *p++ = 0x41;
//     *p++ = 0x5B;

//     *p++ = 0x41;
//     *p++ = 0x5A;

//     *p++ = 0x41;
//     *p++ = 0x59;

//     *p++ = 0x41;
//     *p++ = 0x58;

//     *p++ = 0x5A; // pop rdx

//     // // THE CRUX MOVE: storing the thing
//     // // mov [rax + rdx*4], rsi
//     // *p++ = 0x89;
//     // *p++ = 0x34;
//     // *p++ = 0x10;

//     // // set RAX to 0 (NULL);
//     // *p++ = 0x48;
//     // *p++ = 0x31;
//     // *p++ = 0xc0;
//     // *p++ = 0xc3;

//     // increment gs.rec_size (rdx)
//     // inc edx
//     *p++ = 0xff; // inc instruction
//     *p++ = 0xc2; // ModRM: 11 000 010 (rdx)

//     // mov [rbx], edx  (store back to gs.rec_size)
//     *p++ = 0x89; // mov to memory (32-bit)
//     *p++ = 0x13; // ModRM: 00 010 011 (rdx source, rbx base)

//     // jump all remaining instructions
//     *p++ = 0xEB;
//     *p = 0x00 | (CHUNK - (p - s + 1));
//     // printf("Num is %lu\n", (CHUNK - (p - s + 1)));

//     return CHUNK;
// }

// size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
// {
//     void *help_unmap_addr = (void *)&help_unmap; // We'll call realloc through a helper function
//     unsigned char *p = zero + offset;
//     unsigned char *s = p;

//     // load the address of gs.rec_size into rbx
//     *p++ = 0x48; // REX.W prefix for 64-bit operation
//     *p++ = 0xbb; // mov rbx, imm64
//     uint64_t rec_size_addr = (uint64_t)&gs.rec_size;
//     memcpy(p, &rec_size_addr, sizeof(rec_size_addr));
//     p += 8;

//     // Load rec_size into rdx (rec_size will stay in rdx throughout)
//     *p++ = 0x48; // REX.W
//     *p++ = 0x8b; // mov
//     *p++ = 0x13; // ModR/M: dereference [rbx] into rdx

//     // Load rec_cap into rsi (will be second arg to realloc)
//     *p++ = 0x48; // REX.W
//     *p++ = 0x8b; // mov
//     *p++ = 0x73; // ModR/M byte
//     *p++ = 0x04; // Displacement of 4 bytes (rec_cap is 4 bytes after rec_size)

//     // Compare rec_size and rec_cap
//     *p++ = 0x39; // cmp instruction (32-bit)
//     *p++ = 0xf2; // ModR/M: compare rsi (cap) with rdx (size)

//     // If not equal, jump over realloc section
//     *p++ = 0x75; // jne
//     *p++ = 0x30; // Jump distance (we'll need to calculate this precisely)


//     //______________________________

//     // Double rec_cap (add rsi to itself)
//     *p++ = 0x01; // add
//     *p++ = 0xf6; // ModR/M: add rsi to rsi

//     // Store new cap back to memory
//     *p++ = 0x48; // REX.W
//     *p++ = 0x89; // mov
//     *p++ = 0x73; // ModR/M
//     *p++ = 0x04; // Displacement of 4

//     // Load rec_ids pointer into rdi (will be first arg to realloc)
//     *p++ = 0x48; // REX.W
//     *p++ = 0x8b; // mov
//     *p++ = 0x7b; // ModR/M
//     *p++ = 0xf8; // -8 displacement (rec_ids is 8 bytes before rec_size)

//     // Multiply capacity by 4 (sizeof(uint32_t))
//     *p++ = 0x48; // REX.W
//     *p++ = 0xc1; // shl
//     *p++ = 0xe6; // ModR/M: shift rsi
//     *p++ = 0x02; // by 2 (multiply by 4)

//     // Save registers that we need to preserve
//     *p++ = 0x52; // push rdx (rec_size)
//     *p++ = 0x41;
//     *p++ = 0x50; // push r8
//     *p++ = 0x41;
//     *p++ = 0x51; // push r9
//     *p++ = 0x41;
//     *p++ = 0x52; // push r10
//     *p++ = 0x41;
//     *p++ = 0x53; // push r11

//     // Call realloc
//     *p++ = 0x48; // REX.W
//     *p++ = 0xb8; // mov rax, imm64
//     memcpy(p, &help_unmap_addr, sizeof(void *));
//     p += sizeof(void *);
//     *p++ = 0xff;
//     *p++ = 0xd0; // call rax

//     // Restore registers
//     *p++ = 0x41;
//     *p++ = 0x5b; // pop r11
//     *p++ = 0x41;
//     *p++ = 0x5a; // pop r10
//     *p++ = 0x41;
//     *p++ = 0x59; // pop r9
//     *p++ = 0x41;
//     *p++ = 0x58; // pop r8
//     *p++ = 0x5a; // pop rdx

//     // Store new rec_ids pointer
//     *p++ = 0x48; // REX.W
//     *p++ = 0x89; // mov
//     *p++ = 0x43; // ModR/M
//     *p++ = 0xf8; // -8 displacement


//     //_________________________________________
//     // JUMP LANDING POINT HERE
//     // Store segmentId in rec_ids[rec_size]

//     // // Move segment ID (in rC) to rsi
//     // *p++ = 0x44;            // REX.R prefix for r8-r15
//     // *p++ = 0x89;            // mov
//     // *p++ = 0xc6 | (c << 3); // ModR/M for moving rC to rsi

//     // // Load rec_ids pointer
//     // *p++ = 0x48; // REX.W
//     // *p++ = 0x8b; // mov
//     // *p++ = 0x7b; // ModR/M
//     // *p++ = 0xf8; // -8 displacement


//     // // Store segmentId at rec_ids[rec_size]
//     // *p++ = 0x89; // mov
//     // *p++ = 0x34; // ModR/M: use scaled index
//     // *p++ = 0x97; // SIB: use rdx*4 + rdi

//     // // set RAX to 0 (NULL);
//     // *p++ = 0x48;
//     // *p++ = 0x31;
//     // *p++ = 0xc0;
//     // *p++ = 0xc3;
//     // First get rec_ids pointer in rax
//     *p++ = 0x48; // REX.W
//     *p++ = 0x8b; // mov
//     *p++ = 0x43; // ModR/M
//     *p++ = 0xf8; // -8 displacement from rbx

//     // Calculate offset: rdx * 4 into rdx
//     *p++ = 0x48; // REX.W
//     *p++ = 0x8d; // lea
//     *p++ = 0x14; // ModR/M for rdx destination
//     *p++ = 0x95; // SIB: scale rdx by 4, no base
//     *p++ = 0x00; // displacement 0
//     *p++ = 0x00;
//     *p++ = 0x00;
//     *p++ = 0x00;

//     // Add base pointer to offset
//     *p++ = 0x48; // REX.W
//     *p++ = 0x01; // add
//     *p++ = 0xc2; // ModR/M: add rax to rdx

//     // Move segment ID (in rC) to rsi
//     *p++ = 0x44;            // REX.R prefix for r8-r15
//     *p++ = 0x89;            // mov
//     *p++ = 0xc6 | (c << 3); // ModR/M for moving rC to rsi

//     // // // set RAX to 0 (NULL);
//     // *p++ = 0x48;
//     // *p++ = 0x31;
//     // *p++ = 0xc0;
//     // *p++ = 0xc3;

//     // Store segmentId at calculated address
//     // *p++ = 0x89; // mov
//     // *p++ = 0x32; // ModR/M: store esi to [rdx]
//     *p++ = 0x89; // mov
//     *p++ = 0x02; // ModR/M: 00 000 010 - store esi (now in reg field) to [rdx]

//     // Increment rec_size
//     *p++ = 0xff; // inc
//     *p++ = 0xc2; // inc rdx

//     // Store back to gs.rec_size
//     *p++ = 0x89; // mov
//     *p++ = 0x13; // ModR/M: store rdx to [rbx]

//     // Final jump to skip remaining space
//     *p++ = 0xEB; // jmp rel8
//     *p = 0x00 | (CHUNK - (p - s + 1));

//     return CHUNK;
// }

size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    void *unmap_segment_addr = (void *)&unmap_segment;

    unsigned char *p = zero + offset;
    unsigned char *s = p;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x44;            // Reg prefix for r8-r15
    *p++ = 0x89;            // mov reg to reg
    *p++ = 0xc7 | (c << 3); // ModR/M byte

    // push r8 onto the stack
    *p++ = 0x41;
    *p++ = 0x50;

    *p++ = 0x41;
    *p++ = 0x51;

    *p++ = 0x41;
    *p++ = 0x52;

    *p++ = 0x41;
    *p++ = 0x53;

    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &unmap_segment_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xff;
    *p++ = 0xd0; // ModR/M byte for call rax

    // pop r8 off the stack
    *p++ = 0x41;
    *p++ = 0x5B;

    *p++ = 0x41;
    *p++ = 0x5A;

    *p++ = 0x41;
    *p++ = 0x59;

    *p++ = 0x41;
    *p++ = 0x58;

    // jump 19 bytes
    // jump 19 + 24 = 43 bytes
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));
    // *p++ = 0x2B;

    return CHUNK;
}

// inject segmented load
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    unsigned char *p = zero + offset;
    unsigned char *s = p;

    // load address of val_seq into RAX
    *p++ = 0x48;
    *p++ = 0xb8;
    uint64_t addr = (uint64_t)&gs.val_seq;
    memcpy(p, &addr, sizeof(addr));
    p += 8;

    // mov rax, [rax]            (Dereference to get the value)
    *p++ = 0x48;
    *p++ = 0x8b;
    *p++ = 0x00;

    // mov rdi, rBd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc7 | (b << 3);

    // mov rsi, rCd
    *p++ = 0x44;
    *p++ = 0x89;
    *p++ = 0xc6 | (c << 3);

    // mov rax, [rax + rdi*8]  (using rdi now for *8)
    *p++ = 0x48;
    *p++ = 0x8b;
    *p++ = 0x04;
    *p++ = 0xf8;

    // // mov eax, [rax + rsi*4]  (using rsi now for *4)
    *p++ = 0x8b;
    *p++ = 0x04;
    *p++ = 0xb0;

    // mov to return register
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | a;

    // 36 No Ops
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));

    return CHUNK;
}

// inject segmented store
/* I discovered a weakness in the benchmark that allows me to inline this
 * and get a fair bit more performance out of the compiler. If a UM program
 * were to encounter a segmented store that stored an instruction into the zero
 * segment that was going to be executed, this compiler would crash. However,
 * neither the midmark or the sandmark demand this of the JIT. Perhaps advent
 * or codex does, I haven't checked. If that were the case, I could modify this
 * function to make a function call with the 14 remaining bytes I have. */
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    // void *val_seq_addr = (void *)&gs.val_seq;
    unsigned char *p = zero + offset;
    unsigned char *s = p;

    *p++ = 0x48;
    *p++ = 0xb8;
    uint64_t addr = (uint64_t)&gs.val_seq;
    memcpy(p, &addr, sizeof(addr));
    p += 8;

    // mov rax, [rax]            (Dereference to get the value)
    *p++ = 0x48;
    *p++ = 0x8b;
    *p++ = 0x00;

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
    // *p++ = 0xEB;
    // *p++ = 0x09;
    
    // 64 - 28 = 36
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));
    // *p++ = 0x00 | (CHUNK - (p - s));
    // *p++ = 0x24;

    return CHUNK;
}

// TODO: this improved version is in progress
// inject load program
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    void *load_program_addr = (void *)&load_program;

    unsigned char *p = zero + offset;
    // unsigned char *s = p;

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

    // load the memory address we are working with into rax
    *p++ = 0x48;
    *p++ = 0xb8;
    uint64_t addr = (uint64_t)&gs.pc;
    memcpy(p, &addr, sizeof(addr));
    p += 8;

    // mov [rax], esi
    *p++ = 0x89;
    *p++ = 0x30;

    // // NOTE: trying to move directly from reg c to [rax]
    // *p++ = 0x44;
    // *p++ = 0x89;
    // *p++ = 0x00 | (c << 3);

    // test %edi, %edi  (test if b_val is 0)
    *p++ = 0x85;
    *p++ = 0xff;

    // jnz slow_path
    *p++ = 0x75;
    *p++ = 0x05; // Jump offset to slow path

    // mov 4(%rax), %rax   ; load from [rax + 4] into rax
    *p++ = 0x48;
    *p++ = 0x8b;
    *p++ = 0x40; // ModRM byte for [rax + disp8]
    *p++ = 0x04; // 4 byte displacement

    // Fast path (b_val == 0): gs.active is already in rax
    // ret
    *p++ = 0xc3;

    // slow path below

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