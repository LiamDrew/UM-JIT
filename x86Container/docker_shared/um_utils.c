#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "um_utils.h"
#include "arpa/inet.h"
#include <string.h>
#include <sys/mman.h>

// __attribute__((target("no-r8,no-r9,no-r10,no-r11,no-r12,no-r13,no-r14,no-r15"))) 
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

unsigned char read_char(void)
{
    // printf("Reading in char\n");
    // printf("Global state seq size: %d\n", gs.seq_size);
    int x = getc(stdin);
    assert(x != EOF);
    return (unsigned char)x;
}

// 100% have to push some registers when this function gets called.. be ready
// plus will have to test
uint32_t map_segment(uint32_t size)
{
    // Not absolutely positive if these are necessary
    // Inline assembly to save r8, r9, r10, r11
    __asm__ __volatile__(
        "pushq %%r8\n\t"
        "pushq %%r9\n\t"
        "pushq %%r10\n\t"
        "pushq %%r11\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "memory");

    // print_registers();
    // printf("Mapping segment\n");
    printf("Mapping segment, Raw value is %u (0x%x)\n", size, size);

    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (gs.rec_size == 0) {
        /* Expand if necessary */
        if (gs.seq_size == gs.seq_cap) {
            gs.seq_cap *= 2;

            // realloc the array that keeps track of sequence size
            gs.seg_lens = realloc(gs.seg_lens, gs.seq_cap * sizeof(uint32_t));

            // program sequence is now a sequence of void pointers
            gs.program_seq = realloc(gs.program_seq, gs.seq_cap * sizeof(void*));

            // also need to init the memory segment
            gs.val_seq = realloc(gs.val_seq, gs.seq_cap * sizeof(uint32_t*));


            // Initializing all reallocated memory
            // this may not be strictly necessary
            for (uint32_t i = gs.seq_size; i < gs.seq_cap; i++) {
                gs.program_seq[i] = NULL;
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
    if (gs.program_seq[new_seg_id] == NULL || size > gs.seg_lens[new_seg_id]) {

        // TODO: this step needs to get done with an mmap call
        // gs.program_seq[new_seg_id] = realloc(gs.program_seq[new_seg_id], size * CHUNK);

        gs.program_seq[new_seg_id] = mmap(gs.program_seq[new_seg_id], size * CHUNK, 
            PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        gs.val_seq[new_seg_id] = realloc(gs.val_seq[new_seg_id], size * sizeof(uint32_t*));

        gs.seg_lens[new_seg_id] = size;
    }

    /* zero out the segment */
    memset(gs.program_seq[new_seg_id], 0, size * CHUNK);
    memset(gs.val_seq[new_seg_id], 0, size * sizeof(uint32_t));

    // Inline assembly to restore r8, r9, r10, r11
    __asm__ __volatile__(
        "popq %%r11\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        :
        :
        : "r8", "r9", "r10", "r11");
 
    return new_seg_id;
}

// void unmap segment(void *segmentId)
void unmap_segment(uint32_t segmentId)
{
    // printf("Unmapping segment\n");
    // printf("Seg id is: %u\n", segmentId);
    // TODO: do the unmapping

    __asm__ __volatile__(
        "pushq %%r8\n\t"
        "pushq %%r9\n\t"
        "pushq %%r10\n\t"
        "pushq %%r11\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "memory");
    
    if (gs.rec_size == gs.rec_cap) {
        gs.rec_cap *= 2;
        gs.rec_ids = realloc(gs.rec_ids, gs.rec_cap * sizeof(uint32_t));
    }

    gs.rec_ids[gs.rec_size++] = segmentId;

    __asm__ __volatile__(
        "popq %%r11\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        :
        :
        : "r8", "r9", "r10", "r11");
}

// segmented load
uint32_t segmented_load(uint32_t b_val, uint32_t c_val)
{
    // printf("Segmented load\n");
    // printf("Reg b is %u\n", b_val);
    // printf("Reg c is %u\n", c_val);

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
    __asm__ __volatile__(
        "pushq %%r8\n\t"
        "pushq %%r9\n\t"
        "pushq %%r10\n\t"
        "pushq %%r11\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "memory");
    
    printf("Segmented store: storing cval: %u in segment %u at index %u\n", c_val, a_val, b_val);

    // Midmark makes it to the execution of this before anything goes wrong.
    // A segmented store is happening. this is absolutely crucial for this to go right
    // assert(false);
    // In this case, we have to both compile to machine code and store the native UM word
    // we have compile instruction available to us. We just need to give it a memory segment and an offset.

    // and of course it will be easy to load something straight into memory
    gs.val_seq[a_val][b_val] = c_val;

    // compile step


    // need to be able to handle the case where the instuction we are trying to
    // compile is actually garbage

    // TODO: this is likely to be a pain point for the program.
    compile_instruction(gs.program_seq[a_val], c_val, b_val);

    __asm__ __volatile__(
        "popq %%r11\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        :
        :
        : "r8", "r9", "r10", "r11");

    // print_registers();

    // assert(false);

    return;
}

// load program
/* Load program needs to do something rather important:
 * update the memory address of the new segment being executed
 */
void *load_program(uint32_t b_val, uint32_t c_val)
{
    __asm__ __volatile__(
        "pushq %%r8\n\t"
        "pushq %%r9\n\t"
        "pushq %%r10\n\t"
        "pushq %%r11\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "memory");

    // printf("Loading segment %u, setting program counter to %u\n", b_val, c_val);
    // set program counter to the contents of register c
    gs.pc = c_val;

    // This is a huge benefit of the jit. This step is insanely fast
    if (b_val == 0) {
        return gs.program_seq[0];
    }

    // the right way to do it:
    // void *new_zero = mmap(gs.program_seq[0], gs.seg_lens[b_val] * CHUNK, 
    //     PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    // try this first
    void *new_zero = mmap(NULL, gs.seg_lens[b_val] * CHUNK,
                          PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    memcpy(new_zero, gs.program_seq[b_val], gs.seg_lens[b_val] * CHUNK);

    __asm__ __volatile__(
        "popq %%r11\n\t"
        "popq %%r10\n\t"
        "popq %%r9\n\t"
        "popq %%r8\n\t"
        :
        :
        : "r8", "r9", "r10", "r11");

    return new_zero;

    // return gs.program_seq[b_val];
    // something is not right with b_val
    // return gs.program_seq[0];
}

size_t compile_instruction(void *zero, Instruction word, size_t offset)
{

    // register long r8 asm("r8");
    // register long r9 asm("r9");
    // register long r10 asm("r10");
    // register long r11 asm("r11");
    // register long r12 asm("r12");
    // register long r13 asm("r13");
    // register long r14 asm("r14");
    // register long r15 asm("r15");

    // (void)r8;
    // (void)r9;
    // (void)r10;
    // (void)r11;
    // (void)r12;
    // (void)r13;
    // (void)r14;
    // (void)r15;
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
        // printf("Load value a: %u, b: %u, c: %u\n", a, b, c);
        // Load the right register and do the thing
        offset += load_reg(zero, offset, a + RO, val);
    }

    /* Output */
    else if (opcode == 10)
    {
        // Load the rigther register and do the thing
        // printf("Output a: %u, b: %u, c: %u\n", a, b, c);
        offset += print_reg(zero, offset, c + RO);
    }

    /* Addition */
    else if (opcode == 3)
    {
        // Load the right registers and do the thing
        // printf("Addition a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Halt */
    else if (opcode == 7)
    {
        // printf("Halt a: %u, b: %u, c: %u\n", a, b, c);
        offset += handle_halt(zero, offset);
    }

    /* Bitwise NAND */
    else if (opcode == 6)
    {
        // printf("Bitwise NAND a: %u, b: %u, c: %u\n", a, b, c);
        offset += nand_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Addition */
    else if (opcode == 3)
    {
        // printf("Addtion a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Multiplication */
    else if (opcode == 4)
    {
        // printf("Multiplication a: %u, b: %u, c: %u\n", a, b, c);
        offset += mult_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Division */
    else if (opcode == 5)
    {
        // printf("Division a: %u, b: %u, c: %u\n", a, b, c);
        offset += div_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Conditional Move */
    else if (opcode == 0)
    {
        // printf("Conditional move a: %u, b: %u, c: %u\n", a, b, c);
        offset += cond_move(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Input */
    else if (opcode == 11)
    {
        // printf("Input a: %u, b: %u, c: %u\n", a, b, c);
        offset += read_into_reg(zero, offset, c + RO);
    }

    /* Segmented Load */
    else if (opcode == 1)
    {
        // printf("Segmented load a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_seg_load(zero, offset, a + RO, b + RO, c + RO, word);
    }

    /* Segmented Store */
    else if (opcode == 2)
    {
        // printf("Segmented store a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_seg_store(zero, offset, a + RO, b + RO, c + RO, word);
    }

    /* Load Program */
    else if (opcode == 12)
    {
        // printf("Load progam a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_load_program(zero, offset, b + RO, c + RO);
    }

    /* Map Segment */
    else if (opcode == 8)
    {
        // printf("Map segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_map_segment(zero, offset, b + RO, c + RO);
    }

    /* Unmap Segment */
    else if (opcode == 9)
    {
        // printf("Unmap segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_unmap_segment(zero, offset, c + RO);
    }

    /* Invalid Opcode*/
    else
    {
        // printf("Just a number\n");
        // printf("Opcode: %d\n", opcode);
        // assert(false);
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
    // printf("Printing out x is %u\n", x);
    unsigned char c = (unsigned char)x;
    // printf("Unsigned char is %c\n", c);

    putchar(c);
}

size_t print_reg(void *zero, size_t offset, unsigned c)
{
    // Remove this check when JIT is running
    if (c < 8 || c > 15)
        assert(false);

    void *putchar_addr = (void *)&putchar;
    // void *putchar_addr = (void *)&print_out;
    

    unsigned char *p = zero + offset;

    // xor rdi, rdi
    *p++ = 0x48;
    *p++ = 0x31;
    *p++ = 0xC7;

    // mov edi, rXd (where X is reg_num)
    *p++ = 0x44; // Reg prefix for r8-r15
    // *p++ = 0x89; // mov reg to reg
    *p++ = 0x89;

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = (0xc7 | ((c - 8) << 3)); // ModR/M byte: edi(111) with reg number

    /* This was a nightmare bug*/
    // // push r8 onto the stack
    *p++ = 0x41; // REX.B prefix for r8
    *p++ = 0x50; // PUSH r8

    // call putchar
    *p = 0xe8;
    p++;

    int32_t call_offset = (int32_t)((unsigned char *)putchar_addr - (p + 4));
    memcpy(p, &call_offset, sizeof(int32_t));
    p += sizeof(int32_t);

    // // Pop r8 after putchar returns
    *p++ = 0x41; // REX.B prefix for r8
    *p++ = 0x58; // POP r8

    /* 3 to load reg into edi, 1 for call instruction, 4 for putchar addr */

    // 5 NoOPs to align with chunk boundary
    // *p++ = 0x0F;
    // *p++ = 0x1F;
    // *p++ = 0x00;
    // *p++ = 0x90;
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

    // ret
    *p++ = 0xc3;

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

    // // xor rax, rax
    // *p++ = 0x48;
    // *p++ = 0x31;
    // *p++ = 0xC0;

    // // Push r8 before calling getc/read_char
    // *p++ = 0x41; // REX.B prefix for r8
    // *p++ = 0x50; // PUSH r8

    // Since we're using PIC, let's use a direct relative call
    // This will be a 5-byte instruction: E8 + 32-bit offset
    int32_t rel_offset = (int32_t)((uint64_t)read_char_addr - ((uint64_t)p + 5));

    // call rel32
    *p++ = 0xE8; // Direct relative call
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // mov rCd, eax
    *p++ = 0x49;
    *p++ = 0x89;
    *p++ = 0xC0 | (reg - 8);

    // // Pop r8 after read_char returns
    // *p++ = 0x41; // REX.B prefix for r8
    // *p++ = 0x58; // POP r8

    // TODO: very surpised this isn't crashing

    // 8 No ops
    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
    *p++ = 0x90;
    *p++ = 0x90;
    *p++ = 0x90;
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


    // // push r8
    // *p++ = 0x41; // REX.B prefix for r8
    // *p++ = 0x50; // PUSH r8

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

    // //pop r8
    // *p++ = 0x41; // REX.B prefix for r8
    // *p++ = 0x58; // POP r8

    // 1 No Ops
    *p++ = 0x90;
    *p++ = 0x90;
    *p++ = 0x90;
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

    // push r8
    *p++ = 0x41; // REX.B prefix for r8
    *p++ = 0x50; // PUSH r8

    int32_t rel_offset = (int32_t)((uint64_t)unmap_segment_addr - ((uint64_t)p + 5));

    *p++ = 0xe8;
    memcpy(p, &rel_offset, sizeof(rel_offset));
    p += sizeof(rel_offset);

    // no return value from the unmap segment function

    // pop r8
    *p++ = 0x41; // REX.B prefix for r8
    *p++ = 0x58; // POP r8

    // 4 No Ops

    *p++ = 0x0F;
    *p++ = 0x1F;
    *p++ = 0x00;
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

    // assert(b == 8);

    void *load_program_addr = (void *)&load_program;

    unsigned char *p = zero + offset;

    *p++ = 0x48;
    *p++ = 0x31;
    *p++ = 0xFF;

    // stash b in the right register (even if 0, need to update program pointer)
    // move b to rdi
    *p++ = 0x4C;
    *p++ = 0x89;
    *p++ = 0xc7 | ((b - 8) << 3);

    // *p++ = 0x4C;
    // *p++ = 0x89;
    // *p++ = 0xc7;

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


    // 1 No ops
    // *p++ = 0x90;
    // *p++ = 0x90;
    // *p++ = 0x90;
    *p++ = 0x90;

    // ret
    *p++ = 0xc3;

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