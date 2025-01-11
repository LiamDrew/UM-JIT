#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>

#define CHUNK 40
#define MULT (CHUNK / 4)
#define OPS 15
#define ICAP 32500

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

typedef struct
{
    uint32_t asi;
    uint32_t ai;
    uint32_t bsi;
    uint32_t bi;
    uint32_t csi;
    uint32_t ci;
} OpcodeUpdate;

struct MachineCode
{
    unsigned char bank[CHUNK * OPS];
    OpcodeUpdate ou[OPS + 1];
    uint32_t seg_bytes[OPS];
};

struct GlobalState gs;
struct MachineCode mc;

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
void handle_realloc();
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c);

void print_out(uint32_t x);
size_t print_reg(void *zero, size_t offset, unsigned c);

unsigned char read_char(void);
size_t read_into_reg(void *zero, size_t offset, unsigned c);

void *load_program(uint32_t b_val, uint32_t c_val);
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

    /* Initializing the global state variables */
    uint32_t in_v[] = {11, 31, 31, 11, 11, 14, 14, 4, 36, 33, 33, 17, 40, 0, 0};
    memcpy(mc.seg_bytes, in_v, sizeof(in_v));
    
    // Setting the program counter to 0
    gs.pc = 0;

    /* Initializing the size and capacity of the memory segment array */
    gs.seq_size = 0;
    gs.seq_cap = ICAP;

    /* Sequence of executable memory segments */
    gs.active = NULL;

    /* Sequence of UM words segments (needed for loading and storing) */
    gs.val_seq = calloc(gs.seq_cap, sizeof(uint32_t *));

    /* Array of segment sizes */
    gs.seg_lens = calloc(gs.seq_cap, sizeof(uint32_t));

    /* Initializing the size and capacity of the recycled segments array */
    gs.rec_size = 0;
    gs.rec_cap = ICAP;

    /* Sequence of recycled segment IDs */
    gs.rec_ids = calloc(gs.rec_cap, sizeof(uint32_t));

    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0)
    {
        fsize = file_stat.st_size;
        assert((fsize % 4) == 0);
    }

    /* This function hardcodes the addresses of a, b, and c that need to get
     * injected. They are stored in the MachineCode struct */
    initialize_instruction_bank();

    /* Initialize executable and non-executable memory for the zero segment */
    void *zero = initialize_zero_segment((fsize / 4) * CHUNK);

    // TODO: update this block to change the way size is stored
    uint32_t zero_seg_size = (fsize / 4) + 1;
    uint32_t *zero_vals = calloc(zero_seg_size, sizeof(uint32_t));
    load_zero_segment(zero, zero_vals, fp, zero_seg_size);
    gs.val_seq[0] = zero_vals;

    (void)gs.seg_lens;
    // gs.seg_lens[0] = zero_seg_size;
    gs.seq_size++;
    gs.active = zero;

    void *curr_seg = zero;

    /* Zero out all registers r8-r15 for JIT use */
    asm volatile(
        "movq $0, %%r8\n\t"
        "movq $0, %%r9\n\t"
        "movq $0, %%r10\n\t"
        "movq $0, %%r11\n\t"
        "movq $0, %%r12\n\t"
        "movq $0, %%r13\n\t"
        "movq $0, %%r14\n\t"
        "movq $0, %%r15\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" // Clobber list
    );

    while (curr_seg != NULL)
    {
        Function func = (Function)(curr_seg + (gs.pc * CHUNK));
        curr_seg = func();
    }

    /* Free all program segments */
    for (uint32_t i = 0; i < gs.seq_size; i++)
    {
        free(gs.val_seq[i]);
    }

    free(gs.val_seq);
    free(gs.seg_lens);
    free(gs.rec_ids);

    fclose(fp);
    return 0;
}

void initialize_instruction_bank()
{
    unsigned char *bank = mc.bank;
    uint32_t offset = 0;

    unsigned i = 0;

    // Conditional Move
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 8;

    mc.ou[i].bsi = offset + 8;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 2;
    i++;

    offset += cond_move(bank, offset, 0, 0, 0);

    // Segmented Load
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 28;

    mc.ou[i].bsi = offset + 15;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 18;
    mc.ou[i].ci = 0;
    i++;

    offset += seg_load(bank, offset, 0, 0, 0);

    // Segmented Store
    mc.ou[i].asi = offset + 15;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = offset + 18;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 21;
    mc.ou[i].ci = 0;
    i++;

    offset += seg_store(bank, offset, 0, 0, 0);

    // Addition
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 8;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 5;
    mc.ou[i].ci = 0;
    i++;

    offset += add_regs(bank, offset, 0, 0, 0);

    // Multiplication
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 8;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 5;
    i++;

    offset += mult_regs(bank, offset, 0, 0, 0);

    // Division
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 11;

    mc.ou[i].bsi = offset + 5;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 8;
    i++;

    offset += div_regs(bank, offset, 0, 0, 0);

    // Bitwise NAND
    mc.ou[i].asi = 0;
    mc.ou[i].ai = offset + 11;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 5;
    mc.ou[i].ci = 0;
    i++;

    offset += nand_regs(bank, offset, 0, 0, 0);

    // Halt
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;

    offset += handle_halt(bank, offset);

    // Map Segment
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = offset + 33;

    mc.ou[i].csi = offset + 2;
    mc.ou[i].ci = 0;
    i++;

    offset += inject_map_segment(bank, offset, 0, 0);

    // Unmap Segment
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    // mc.ou[i].csi = offset + 56; // Unrolled
    mc.ou[i].csi = offset + 2; // Rolled
    mc.ou[i].ci = 0;
    i++;

    offset += inject_unmap_segment(bank, offset, 0);

    // Output
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 2;
    mc.ou[i].ci = 0;
    i++;

    offset += print_reg(bank, offset, 0);

    // Input
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = offset + 14;
    i++;

    offset += read_into_reg(bank, offset, 0);

    // Load Program
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = offset + 2;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = offset + 5;
    mc.ou[i].ci = 0;
    i++;

    offset += inject_load_program(bank, offset, 0, 0);

    // Load Value
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;

    offset += CHUNK;

    // Opcode 14
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;
    offset += CHUNK;

    // Opcode 15
    mc.ou[i].asi = 0;
    mc.ou[i].ai = 0;

    mc.ou[i].bsi = 0;
    mc.ou[i].bi = 0;

    mc.ou[i].csi = 0;
    mc.ou[i].ci = 0;
    i++;

    offset += CHUNK;
}

void *initialize_zero_segment(size_t asmbytes)
{
    void *zero = mmap(NULL, asmbytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zero != MAP_FAILED);

    memset(zero, 0, asmbytes);
    return zero;
}

void load_zero_segment(void *zero, uint32_t *zero_vals, FILE *fp, size_t zero_seg_size)
{
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;
    size_t offset = 0;

    (void)zero_seg_size;
    zero_vals[0] = zero_seg_size;

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

            // TODO: change the location the word gets inserted into the val array
            // zero_vals[i / 4] = word;
            zero_vals[(i / 4) + 1] = word;

            /* At this point, the word is assembled and ready to be compiled
             * into assembly */
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

// An alternate (less optimized) implementation of compile instruction
size_t compile_instruction(void *zero, Instruction word, size_t offset)
{
    uint32_t opcode = (word >> 28) & 0xF;
    uint32_t a = 0;

    // Now based on the opcode, figure out what to do

    // Load Value
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

    // Output
    if (opcode == 10)
    {
        // Load the rigther register and do the thing
        // printf("Output a: %u, b: %u, c: %u\n", a, b, c);
        offset += print_reg(zero, offset, c);
    }

    // Addition
    else if (opcode == 3)
    {
        // Load the right registers and do the thing
        // printf("Addition a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a, b, c);
    }

    // Halt
    else if (opcode == 7)
    {
        // printf("Haslt a: %u, b: %u, c: %u\n", a, b, c);
        offset += handle_halt(zero, offset);
    }

    // Bitwise NAND
    else if (opcode == 6)
    {
        // printf("Bitwise NAND a: %u, b: %u, c: %u\n", a, b, c);
        offset += nand_regs(zero, offset, a, b, c);
    }

    // Addition
    else if (opcode == 3)
    {
        // printf("Addtion a: %u, b: %u, c: %u\n", a, b, c);
        offset += add_regs(zero, offset, a, b, c);
    }

    // Multiplication
    else if (opcode == 4)
    {
        // printf("Multiplication a: %u, b: %u, c: %u\n", a, b, c);
        offset += mult_regs(zero, offset, a, b, c);
    }

    // Division
    else if (opcode == 5)
    {
        // printf("Division a: %u, b: %u, c: %u\n", a, b, c);
        offset += div_regs(zero, offset, a, b, c);
    }

    // Conditional Move
    else if (opcode == 0)
    {
        // printf("Conditional move a: %u, b: %u, c: %u\n", a, b, c);
        offset += cond_move(zero, offset, a, b, c);
    }

    // Input
    else if (opcode == 11)
    {
        // printf("Input a: %u, b: %u, c: %u\n", a, b, c);
        offset += read_into_reg(zero, offset, c);
    }

    // Segmented Load
    else if (opcode == 1)
    {
        // printf("Segmented load a: %u, b: %u, c: %u\n", a, b, c);
        offset += seg_load(zero, offset, a, b, c);
    }

    // Segmented Store
    else if (opcode == 2)
    {
        // printf("Segmented store a: %u, b: %u, c: %u\n", a, b, c);
        offset += seg_store(zero, offset, a, b, c);
    }

    // Load Program
    else if (opcode == 12)
    {
        // printf("Load progam a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_load_program(zero, offset, b, c);
    }

    // Map Segment
    else if (opcode == 8)
    {
        // printf("Map segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_map_segment(zero, offset, b, c);
    }

    // Unmap Segment
    else if (opcode == 9)
    {
        // printf("Unmap segment a: %u, b: %u, c: %u\n", a, b, c);
        offset += inject_unmap_segment(zero, offset, c);
    }

    // Invalid Opcode
    else
    {
        // This value is not an instruction that is meant to be executed
        // Nothing is being written, but we still need a valid offset
        offset += CHUNK;
    }

    return offset;
}

void update_bank(uint32_t a, uint32_t b, uint32_t c, uint32_t opcode)
{
    uint8_t *bank = (uint8_t *)mc.bank;

    OpcodeUpdate ou = mc.ou[opcode];

    if (ou.asi != 0)
    {
        bank[ou.asi] &= ~(0x7 << 3);
        bank[ou.asi] |= (a << 3);
    }
    else if (ou.ai != 0)
    {
        bank[ou.ai] &= ~(0x7);
        bank[ou.ai] |= a;
    }

    if (ou.bsi != 0)
    {
        bank[ou.bsi] &= ~(0x7 << 3);
        bank[ou.bsi] |= (b << 3);
    }
    else if (ou.bi != 0)
    {
        bank[ou.bi] &= ~(0x7);
        bank[ou.bi] |= b;
    }

    if (ou.csi != 0)
    {
        bank[ou.csi] &= ~(0x7 << 3);
        bank[ou.csi] |= (c << 3);
    }
    else if (ou.ci != 0)
    {
        bank[ou.ci] &= ~(0x7);
        bank[ou.ci] |= c;
    }
}
/*
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

    uint32_t lower = opcode * CHUNK;

    // update a, b, and c values within the appropriate ranges
    update_bank(a, b, c, opcode);

    uint32_t copy_size = mc.seg_bytes[opcode];
    memcpy(zero + offset, mc.bank + lower, copy_size);

    offset += CHUNK;
    return offset;
}
*/

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

    // This should automatically jump forward the correct number of bytes
    *p++ = 0xEB;
    *p = 0x1F;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

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

    *p++ = 0x45;
    *p++ = 0x89;
    *p++ = 0xC0 | (b << 3) | a;

    // jump 29 bytes
    *p++ = 0xEB;
    // *p = 0x1D;
    
    (void)s;
    *p = 0x00 | (CHUNK - (p - s + 1));

    return CHUNK;
}

// inject segmented load
size_t seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
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

    // TODO: increment correct register by one when loading
    // TODO: need to increment rsi by 1

    // increment rsi (rC is preserved)
    *p++ = 0xff; // inc
    *p++ = 0xc6; // inc rsi

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
    *p = 0x07;
    
    (void)s;
    // *p = 0x00 | (CHUNK - (p - s + 1));

    return CHUNK;
}

size_t seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c)
{

    unsigned char *p = zero + offset;
    unsigned char *s = p;

    *p++ = 0x48;
    *p++ = 0xb8;
    uint64_t addr = (uint64_t)&gs.val_seq;
    memcpy(p, &addr, sizeof(addr));
    p += sizeof(addr);

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

    // TODO: increment correct register when storing
    // TODO: increment rsi by 1
    // increment rsi (rC is preserved)
    *p++ = 0xff; // inc
    *p++ = 0xc6; // inc rsi

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

    *p++ = 0xEB;
    *p = 0x07;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

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

    // Jump
    *p++ = 0xEB;
    *p = 0x00 | (CHUNK - (p - s + 1));

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

    // jump
    *p++ = 0xEB;
    *p = 0x1D;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

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
    *p = 0x1A;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

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

    // jump
    *p++ = 0xEB;
    *p = 0x1A;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

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

uint32_t map_segment(uint32_t seg_size)
{
    uint32_t new_seg_id;
    uint32_t eff_size = seg_size + 1;
    (void)eff_size;

    // TODO: make the map segment 1 element larger (to store size)

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

    // TODO: should simplify to just free and malloc every time, especially
    // considering the changes that are coming

    /* If the segment didn't previously exist or wasn't large enought for us*/
    // if (gs.val_seq[new_seg_id] == NULL || seg_size > gs.seg_lens[new_seg_id])
    if (gs.val_seq[new_seg_id] == NULL || seg_size > gs.val_seq[new_seg_id][0])
    {
        gs.val_seq[new_seg_id] = realloc(gs.val_seq[new_seg_id], eff_size * sizeof(uint32_t));
        assert(gs.val_seq[new_seg_id] != NULL); // Make sure the realloc didn't fail

        gs.seg_lens[new_seg_id] = eff_size;
    }

    /* zero out the segment */
    memset(gs.val_seq[new_seg_id], 0, eff_size * sizeof(uint32_t));
    gs.val_seq[new_seg_id][0] = seg_size;

    return new_seg_id;
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

    // 12 byte function call
    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &map_segment_addr, sizeof(void *));
    p += sizeof(void *);
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
    *p = 0x04;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

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

    // 12 byte function call
    *p++ = 0x48; // REX.W prefix
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &unmap_segment_addr, sizeof(void *));
    p += sizeof(void *);
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
    *p = 0x07;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

    return CHUNK;
}

/*
// Below is the unrolled version of Map segment. The rolled version might be interesting
void handle_realloc()
{
    // double the capacity
    gs.rec_cap *= 2;
    gs.rec_ids = realloc(gs.rec_ids, gs.rec_cap * sizeof(uint32_t));
    assert(gs.rec_ids != NULL);
}

size_t inject_unmap_segment(void *zero, size_t offset, unsigned c)
{
    // void *realloc_addr = (void *)&realloc;
    void *handle_realloc_addr = (void *)&handle_realloc;
    (void)handle_realloc_addr;

    // printf("In function: Pointer is %p\n", handle_realloc_addr);

    unsigned char *p = zero + offset;
    unsigned char *s = p;

    // load the address of gs.rec_size into rbx
    *p++ = 0x48; // REX.W prefix for 64-bit operation
    *p++ = 0xbb; // mov rbx, imm64
    uint64_t rec_size_addr = (uint64_t)&gs.rec_size;
    memcpy(p, &rec_size_addr, sizeof(rec_size_addr));
    p += 8;

    // Load rec_size into rdx (rec_size will stay in rdx throughout)
    // 32 bit operation
    *p++ = 0x8b; // mov
    *p++ = 0x13; // ModR/M: dereference [rbx] into rdx

    // Load rec_cap into rsi (will be second arg to realloc)
    *p++ = 0x48; // REX.W
    *p++ = 0x8b; // mov
    *p++ = 0x73; // ModR/M byte
    *p++ = 0x04; // Displacement of 4 bytes (rec_cap is 4 bytes after rec_size)

    // Compare rec_size and rec_cap
    *p++ = 0x39; // cmp instruction (32-bit)
    *p++ = 0xf2; // ModR/M: compare rsi (cap) with rdx (size)

    // If not equal, jump over realloc section
    *p++ = 0x75; // jne
    *p++ = 0x1E; // Jump distance (we'll need to calculate this precisely)

    //______________________________
    // Save registers that we need to preserve
    *p++ = 0x52; // push rdx (rec_size)

    *p++ = 0x41;
    *p++ = 0x50; // push r8
    *p++ = 0x41;
    *p++ = 0x51; // push r9
    *p++ = 0x41;
    *p++ = 0x52; // push r10
    *p++ = 0x41;
    *p++ = 0x53; // push r11

    // Call realloc
    *p++ = 0x48; // REX.W
    *p++ = 0xb8; // mov rax, imm64
    memcpy(p, &handle_realloc_addr, sizeof(void *));
    p += sizeof(void *);
    *p++ = 0xff;
    *p++ = 0xd0; // call rax

    // Restore registers
    *p++ = 0x41;
    *p++ = 0x5b; // pop r11
    *p++ = 0x41;
    *p++ = 0x5a; // pop r10
    *p++ = 0x41;
    *p++ = 0x59; // pop r9
    *p++ = 0x41;
    *p++ = 0x58; // pop r8

    *p++ = 0x5a; // pop rdx
    //_________________________________________
    // JUMP LANDING POINT HERE
    // Store segmentId in rec_ids[rec_size]

    // rdi is a pointer
    // First load rec_ids pointer from [rbx - 8] into rdi
    *p++ = 0x48; // REX.W prefix
    *p++ = 0x8b; // mov
    *p++ = 0x7b; // ModRM: 01 111 011 (changed from 0x43 to 0x7b)
    *p++ = 0xf8; // -8 displacement

    // mov [rdi + rdx*4], rC  (directly from rC to memory)
    *p++ = 0x44;              // REX.R prefix for r8-r15
    *p++ = 0x89;              // mov reg to mem
    *p++ = (0x04 | (c << 3)); // ModR/M with SIB and register encoded
    *p++ = 0x97;              // SIB byte (scale=4, index=rdx, base=rdi)

    // Increment rec_size
    *p++ = 0xff; // inc
    *p++ = 0xc2; // inc rdx

    //c = 12
    // 11 000 010
    rax 000
    rcx 001
    rdx 010
    rbx 011
    rsp
    rbp
    rsi
    rdi

    // Store back to gs.rec_size
    *p++ = 0x89; // mov
    *p++ = 0x13; // ModR/M: store rdx to [rbx]

    // Final jump to skip remaining space
    *p++ = 0xEB; // jmp rel8
    *p = 0x00 | (CHUNK - (p - s + 1));

    return CHUNK;
}

*/

size_t print_reg(void *zero, size_t offset, unsigned c)
{
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
    *p = 0x07;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;
    (void)p;
    (void)putchar_addr;
    (void)c;

    // *p++ = 0xEB;
    // *p = 0x26;

    return CHUNK;
}

unsigned char read_char(void)
{
    int x = getc(stdin);
    unsigned char c = (unsigned char)x;
    return c;
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
    *p = 0x17;
    // *p = 0x00 | (CHUNK - (p - s + 1));
    (void)s;

    return CHUNK;
}

void *load_program(uint32_t b_val, uint32_t c_val)
{
    (void)c_val;
    // The following two steps get handled in inline assembly
    /* Set the program counter to be the contents of register c */
    /* If segment zero is loaded, just return the active segment */

    /* If a different segment is loaded, put that in */
    uint32_t new_seg_size = gs.val_seq[b_val][0];
    uint32_t eff_size = new_seg_size + 1;
    uint32_t *new_vals = calloc(eff_size, sizeof(uint32_t));
    memcpy(new_vals, gs.val_seq[b_val], eff_size * sizeof(uint32_t));

    /* Update the existing memory segment */
    gs.val_seq[0] = new_vals;
    // gs.seg_lens[0] = new_seg_size;

    // this function will have to do the compiling for the new memory segment
    void *new_zero = mmap(NULL, new_seg_size * CHUNK,
                          PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memset(new_zero, 0, new_seg_size * CHUNK);

    uint32_t offset = 0;
    // The array is 1 indexed, index 0 contains the size
    for (uint32_t i = 1; i < eff_size; i++)
    {
        offset = compile_instruction(new_zero, new_vals[i], offset);
    }

    gs.active = new_zero;
    return new_zero;
}

size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c)
{
    void *load_program_addr = (void *)&load_program;

    unsigned char *p = zero + offset;

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

    // NOTE: super sus that the registers don't need to be on the stack.. double check this
    // 12 byte function call
    *p++ = 0x48;
    *p++ = 0xb8;
    memcpy(p, &load_program_addr, sizeof(load_program_addr));
    p += sizeof(load_program_addr);
    *p++ = 0xff;
    *p++ = 0xd0;

    // this function better return rax as the right thing
    // injected function needs to ret (rax should already be the right thing)
    // ret
    *p++ = 0xc3;

    return CHUNK;
}

// NOTE: The inline calculation of the jump size costs us some performance. When the assembly
// has been locked in, hardcode the jump size for some extra performance

