#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// TODO: make assembly format consistent (capitalization, spacing, etc.)

#define RO 8

typedef uint32_t Instruction;
typedef int (*Function)(void);

/* Sequence of program segments */
void **segment_sequence = NULL;
uint32_t seq_size = 0;
uint32_t seq_capacity = 0;
uint32_t *segment_lengths = NULL;

/* Sequence of recycled segments */
void *recycled_ids = NULL;
uint32_t rec_size = 0;
uint32_t rec_capacity = 0;

void *initialize_zero_segment(size_t fsize);
uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);
size_t zero_all_registers(void *zero, size_t offset);
void load_zero_segment(void *zero, FILE *fp, size_t fsize);
size_t compile_instruction(void *zero, uint32_t word, size_t offset);
size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value);
size_t print_reg(void *zero, size_t offset, unsigned reg);
size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t handle_halt(void *zero, size_t offset);
size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
unsigned char read_char();
size_t read_into_reg(void *zero, size_t offset, unsigned reg);

uint32_t map_segment(uint32_t size);
size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c);
void unmap_segment(uint32_t segmentID);
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c);
void segmented_load(uint32_t a_val, uint32_t b_val, uint32_t c_val);
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val);
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
void load_program(uint32_t b_val, uint32_t c_val);
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
        fsize = file_stat.st_size;

    printf("Starting program.\n");
    void *zero = initialize_zero_segment(fsize);

    load_zero_segment(zero, fp, fsize);
    /* Counter of bytes written */
    // size_t bw = 0;

    // bw += load_reg(zero, bw, 8, 57);
    // bw += print_edi(zero, bw, 8);

    Function func = (Function)zero;
    int result = func();
    (void)result;

    printf("\nFinished Program.\n");

    /* Free zero segment */
    assert(munmap(zero, fsize) != -1);

    fclose(fp);
    return 0;
}

void *initialize_zero_segment(size_t fsize)
{
    void *zero = mmap(NULL, fsize, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zero != MAP_FAILED);

    memset(zero, 0, fsize);
    return zero;
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

void load_zero_segment(void *zero, FILE *fp, size_t fsize)
{
    (void)fsize;
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;
    size_t offset = 0;
    offset = zero_all_registers(zero, offset);

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

            /* At this point, the word is assembled and ready to be compiled
             * into assembly */
            offset = compile_instruction(zero, word, offset);
            word = 0;
        }
        i++;
    }
}

size_t zero_all_registers(void *zero, size_t offset)
{
    assert(offset == 0);

    unsigned char *p = zero + offset;

    // zeroing out all 8 registers

    // xor r8, r8 (sets it to 0)
    *p++ = 0x4D;
    *p++ = 0x31;
    *p++ = 0xc0;

    // mov r9, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc1;

    // mov r10, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc2;

    // mov r11, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc3;

    // mov r12, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc4;

    // mov r13, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc5;

    // mov r14, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc6;

    // mov r15, r8
    *p++ = 0x4D;
    *p++ = 0x89;
    *p++ = 0xc7;

    return 24;
}

size_t compile_instruction(void *zero, uint32_t word, size_t offset)
{
    uint32_t opcode = (word >> 28) & 0xF;

    printf("Opcode: %d\n", opcode);
    uint32_t a = 0, b = 0, c = 0, val = 0;

    if (opcode == 13) {
        a = (word >> 25) & 0x7;
        val = word & 0x1FFFFFF;
        printf("Reg a is %d\n", a);
        printf("Val to load is %d\n", val);
    } else {
        c = word & 0x7;
        b = (word >> 3) & 0x7;
        a = (word >> 6) & 0x7;
        printf("Reg a is %d\n", a);
        printf("Reg b is %d\n", b);
        printf("Reg c is %d\n", c);
    }

    // Now based on the opcode, figure out what to do

    /* Load Value */
    if (opcode == 13) {
        // Load the right register and do the thing
        offset += load_reg(zero, offset, a + RO, val);
    }

    /* Output */
    else if (opcode == 10) {
        // Load the rigther register and do the thing
        offset += print_reg(zero, offset, c + RO);
    }

    else if (opcode == 3) {
        // Load the right registers and do the thing
        offset += add_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Halt */
    else if (opcode == 7) {
        offset += handle_halt(zero, offset);
    }

    /* Bitwise NAND */
    else if (opcode == 6) {
        offset += nand_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Addition */
    else if (opcode == 3) {
        offset += add_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Multiplication */
    else if (opcode == 4) {
        offset += mult_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Division */
    else if (opcode == 5) {
        offset += div_regs(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Conditional Move */
    else if (opcode == 0) {
        offset += cond_move(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Input */
    else if (opcode == 11) {
        offset += read_into_reg(zero, offset, c + RO);
    }

    /* Segmented Load */
    else if (opcode == 1) {
        offset += inject_seg_load(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Segmented Store */
    else if (opcode == 2) {
        offset += inject_seg_store(zero, offset, a + RO, b + RO, c + RO);
    }

    /* Load Program */
    else if (opcode == 12) {
        offset += inject_load_program(zero, offset, b + RO, c + RO);
    }

    /* Map Segment */
    else if (opcode == 8) {
        offset += inject_map_segment(zero, offset, b + RO, c + RO);
    }

    /* Unmap Segment */
    else if (opcode == 9) {
        offset += inject_unmap_segment(zero, offset, c + RO);
    }

    /* Invalid Opcode*/
    else {
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
    *p++ = 0x41;             // Reg prefix for r8-r15
    *p++ = 0xc7;             // mov immediate value to 32-bit register
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

    return 7;
}

size_t print_reg(void *zero, size_t offset, unsigned reg)
{
    // Remove this check when JIT is running
    if (reg < 8 || reg > 15)
        assert(false);

    void *putchar_addr = (void *)&putchar;

    unsigned char *p = zero + offset;

    // mov edi, rXd (where X is reg_num)
    *p++ = 0x44;                      // Reg prefix for r8-r15
    *p++ = 0x89;                      // mov reg to reg

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
    return 8;
}

size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    // Remove this check when JIT is running
    printf("a: %u, b: %u, c: %u\n", a, b, c);
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
    *p++ = 0x45; // Reg prefix for r8-r15
    *p++ = 0x01; // add reg to reg
    *p++ = 0xc0 | ((c - 8) << 3) | (a - 8); // ModR/M byte

    return 6;
}

size_t handle_halt(void *zero, size_t offset)
{
    unsigned char *p = zero + offset;

    // ret
    *p = 0xc3;
    
    return 1;
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

    return 9;
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

    return 12;
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

    return 9;
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

    return 9;
}

unsigned char read_char()
{
    // printf("Function was called\n");
    int x = getc(stdin);
    assert(x != EOF);
    unsigned char c = (unsigned char)x;
    // putchar(c);
    // printf("\nDone printing\n");
    // printf("Returning char: %d ('%c')\n", c, c);
    return c;
}

size_t read_into_reg(void *zero, size_t offset, unsigned reg)
{
    // Remove this check when JIT is running
    if (reg < 8 || reg > 15)
        assert(false);

    unsigned char *p = zero + offset;

    // void *putchar_addr = (void *)&putchar;
    void *read_char_addr = (void *)&read_char;

    // mov rax, imm64 (function address)
    *p++ = 0x48;
    *p++ = 0xB8;
    memcpy(p, &read_char_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xFF;
    *p++ = 0xD0;

    // mov rCd, eax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xC0 | (reg - 8);

    return 15;
}


// There may need to be two functions for segment mapping and unmapping
// 
// Returns the segment ID which will be stored in register b
// That step will be done by the injected assembly, this function is pure C
uint32_t map_segment(uint32_t size)
{
    (void)size;
    // Size will be stored in register c
    // it will be passed in as an argument to the function

    // mmap something

    // return the address of the mmaped memory
    // return;

    // the injected assembly will store the address in register b
    // Store memory address in register b
    assert(false);
}

size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c)
{
    void *map_segment_addr = (void *)&map_segment;

    unsigned char *p = zero + offset;

    // TODO: the offset method will not work and needs to be fixed

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x48; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = 0xc7 | ((c - 8) << 3); // ModR/M byte

    // mov rax, imm64 (function address)
    *p++ = 0x48;
    *p++ = 0xB8;
    memcpy(p, &map_segment_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xFF;
    *p++ = 0xD0;

    // move return value from rax to reg b
    // mov rB, rax
    *p++ = 0x41;
    *p++ = 0x89;
    *p++ = 0xc0 | (b - 8);

    // store the result in register b
    return 19;
}

// void unmap segment(void *segmentId)
void unmap_segment(uint32_t segmentID)
{
    (void)segmentID;
    assert(false);
}

// size_t inject unmap segment
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    void *unmap_segment_addr = (void *)&unmap_segment;

    unsigned char *p = zero + offset;

    // move reg c to be the function call argument
    // mov rC, rdi
    *p++ = 0x48; // Reg prefix for r8-r15
    *p++ = 0x89; // mov reg to reg

    /* ModR/M byte Format:
     * [7-6: Mod (2 bits)][5-3: Source Reg (3 bits)][2-0: Dest Reg (3 bits)] */
    *p++ = 0xc7 | ((c - 8) << 3); // ModR/M byte

    // mov rax, imm64 (function address)
    *p++ = 0x48;
    *p++ = 0xB8;
    memcpy(p, &unmap_segment_addr, sizeof(void *));
    p += sizeof(void *);

    // call rax
    *p++ = 0xFF;
    *p++ = 0xD0;

    // no return value from the unmap segment function
    return 15;
}

// segmented load 
uint32_t segmented_load(uint32_t a_val, uint32_t b_val, uint32_t c_val)
{

    (void)a_val;
    (void)b_val;
    (void)c_val;
    assert(false);
}

// inject segmented load
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    (void)zero;
    (void)offset;
    (void)a;
    (void)b;
    (void)c;
    assert(false);
    return 0;
    // mov regs a, b, c into the right registers for the function call

    // call the function

    // return
}

// segmented store (will have to compile r[C] to machine code inline)
void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val)
{
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
     */
    (void)a_val;
    (void)b_val;
    (void)c_val;
    return;
}

// inject segmented store
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{
    (void)zero;
    (void)offset;
    (void)a;
    (void)b;
    (void)c;
    assert(false);
    return 0;
}

// load program
void load_program(uint32_t b_val, uint32_t c_val)
{
    (void)b_val;
    (void)c_val;
}

// inject load program
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    (void)zero;
    (void)offset;
    (void)b;
    (void)c;
    assert(false);
    return 0;
}
