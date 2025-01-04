/**
 * @file main.c
 * @author Liam Drew
 * @date 2024-12-27
 * @brief
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/mman.h>

typedef uint32_t Instruction;
typedef void* (*Function)(void);
// typedef void (*Function)(void);


#define SENTINEL ((void *)-1)

_Static_assert((uintptr_t)SENTINEL != (uintptr_t)NULL, "SENTINEL must not equal NULL");

// Program pointer
uint32_t pc = 0;

/* Sequence of program segments */
uint32_t **segment_sequence = NULL;
uint32_t seq_size = 0;
uint32_t seq_capacity = 0;
uint32_t *segment_lengths = NULL;

/* Sequence of recycled segments */
uint32_t *recycled_ids = NULL;
uint32_t rec_size = 0;
uint32_t rec_capacity = 0;

unsigned char read_char();

uint32_t *initialize_memory(FILE *fp, size_t fsize);
uint64_t assemble_word(uint64_t word, unsigned width, unsigned lsb,
                       uint64_t value);

void handle_instructions(uint32_t *zero);
void handle_stop();
static inline bool exec_instr(Instruction word, Instruction **pp,
                              void *active);
uint32_t map_segment(uint32_t size);
void unmap_segment(uint32_t segment);

// OLD
// void load_segment(uint32_t index, uint32_t *zero);

uint32_t segmented_load(uint32_t b_val, uint32_t c_val);
void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val);

void load_program(uint32_t b_val, uint32_t c_val);
// void *load_program(uint32_t b_val, uint32_t c_val);

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
    if (stat(argv[1], &file_stat) == 0) {
        fsize = file_stat.st_size;
        assert(fsize % 4 == 0);
    }

    uint32_t zero_seg_num_words = fsize / 4;
    uint32_t *zero_segment = initialize_memory(fp, zero_seg_num_words);

    handle_instructions(zero_segment);

    handle_stop();
    return EXIT_SUCCESS;
}

void handle_instructions(uint32_t *zero)
{
    // registers done inline
    // program pointer done inline
    // uint32_t regs[NUM_REGISTERS] = {0};


    /* Iterate through every word in the zero segment */
    (void)zero;
    // Instruction *pp = zero;
    Instruction word;

    // printf("About to start doing things\n");

    // however many bytes are necessary
    void *active = mmap(NULL, 40, PROT_READ | PROT_WRITE | PROT_EXEC, 
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(active != MAP_FAILED);
    memset(active, 0, 40);

    // zero all regs 8 - 15
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

    bool exit = false;
    // void *test = NULL;
    // void *sent = ((void *)1);
    // Instruction *curr = zero;
    void *temp = NULL;
    // while (temp == NULL)
    while(!exit)
    {
        word = segment_sequence[0][pc];
        pc++;
        // word = *curr;

        // // word = *pp;
        // // pp++;

        // exit = exec_instr(word, &pp, active);
        exit = exec_instr(word, NULL, active);
        (void)exit;

        //execute the active segment after it has been assembled
        Function func = (Function)active;
        temp = func();
        (void)temp;

        // clear the exec segment after each time it gets executed
        // memset(active, 0, 40);
    }
}

static inline bool exec_instr(Instruction word, Instruction **pp, void *active)
{
    // TODO: may need to get used, not sure yet
    (void)pp;
    uint32_t a = 0, b = 0, c = 0, val = 0;
    uint32_t opcode = word >> 28;
    unsigned char *p = active;

    /* Load Value */
    if (__builtin_expect(opcode == 13, 1))
    {
        a = (word >> 25) & 0x7;
        val = word & 0x1FFFFFF;
        // printf("Load value %u into reg %u\n", val, a);

        /* mov rXd, imm32 (where X is reg_num) */
        *p++ = 0x41;     // Reg prefix for r8-r15
        *p++ = 0xc7;     // mov immediate value to 32-bit register
        *p++ = 0xc0 | a; // ModR/M byte for target register

        *p++ = val & 0xFF;
        *p++ = (val >> 8) & 0xFF;
        *p++ = (val >> 16) & 0xFF;
        *p++ = (val >> 24) & 0xFF;

        // ret
        *p++ = 0xc3;
        return false;
    }

    c = word & 0x7;
    b = (word >> 3) & 0x7;
    a = (word >> 6) & 0x7;

    /* Segmented Load */
    if (__builtin_expect(opcode == 1, 1))
    {
        // TODO: This should be done in inline assembly instead of with a function
        void *seg_load_addr = (void *)segmented_load;

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

        // *p++ = 0x41;
        // *p++ = 0x52;

        // *p++ = 0x41;
        // *p++ = 0x53;

        *p++ = 0x48; // REX.W prefix
        *p++ = 0xb8; // mov rax, imm64
        memcpy(p, &seg_load_addr, sizeof(void *));
        p += sizeof(void *);

        // call rax
        *p++ = 0xff;
        *p++ = 0xd0; // ModR/M byte for call rax

        // pop r8 - r11 off the stack
        // *p++ = 0x41;
        // *p++ = 0x5B;

        // *p++ = 0x41;
        // *p++ = 0x5A;

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

        // ret
        *p++ = 0xc3;
    }

    /* Segmented Store */
    else if (__builtin_expect(opcode == 2, 1))
    {
        // TODO: This should also be done in inline assembly instead of with a function

        void *seg_store_addr = (void *)segmented_store;

        // mov rsi, rad
        *p++ = 0x44;
        *p++ = 0x89;
        *p++ = 0xc7 | (a << 3);

        // mov rdx, rbd
        *p++ = 0x44;
        *p++ = 0x89;
        *p++ = 0xc6 | (b << 3);

        // mov rcx, rcd
        *p++ = 0x44;
        *p++ = 0x89;
        *p++ = 0xc2 | (c << 3);

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
        memcpy(p, &seg_store_addr, sizeof(void *));
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

        // ret
        *p++ = 0xc3;
    }

    /* Bitwise NAND */
    else if (__builtin_expect(opcode == 6, 1))
    {
        // xor rax, rax
        *p++ = 0x48;
        *p++ = 0x31;
        *p++ = 0xc0;

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

        // ret
        *p++ = 0xc3;
    }

    /* Load Program */
    else if (__builtin_expect(opcode == 12, 0))
    {
        // Update the program pointer in this case

        void *load_program_addr = (void *)&load_program;

        // TODO: could potentially do this in inline assembly if I was feeling ambitious

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
    }

    /* Addition */
    else if (__builtin_expect(opcode == 3, 0))
    {
        // xor rax, rax
        *p++ = 0x48;
        *p++ = 0x31;
        *p++ = 0xc0;

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

        // ret
        *p++ = 0xc3;
    }

    /* Conditional Move */
    else if (__builtin_expect(opcode == 0, 0))
    {
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
        // mov rAd, rAd
        *p++ = 0x45;
        *p++ = 0x89;
        *p++ = 0xC0 | (b << 3) | a;

        // ret
        *p++ = 0xc3;
    }

    /* Map Segment */
    else if (__builtin_expect(opcode == 8, 0))
    {
        void *map_segment_addr = (void *)&map_segment;

        // TODO

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

        // ret
        *p++ = 0xc3;

    }

    /* Unmap Segment */
    else if (__builtin_expect(opcode == 9, 0))
    {
        // printf("Unmap segment a: %u, b: %u, c: %u\n", a, b, c);
        // unmap_segment(regs[c]);

        // TODO:
        void *unmap_segment_addr = (void *)&unmap_segment;

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
        memcpy(p, &unmap_segment_addr, sizeof(void *));
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

        // ret
        *p++ = 0xc3;
    }

    /* Division */
    else if (__builtin_expect(opcode == 5, 0))
    {
        // xor rax,rax
        *p++ = 0x48;
        *p++ = 0x31;
        *p++ = 0xc0;

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

        // xor rax,rax
        *p++ = 0x48;
        *p++ = 0x31;
        *p++ = 0xc0;

        // ret
        *p++ = 0xc3;
    }

    /* Multiplication */
    else if (__builtin_expect(opcode == 4, 0))
    {
        // xor rax,rax
        *p++ = 0x48;
        *p++ = 0x31;
        *p++ = 0xc0;

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

        // xor rax,rax
        *p++ = 0x48;
        *p++ = 0x31;
        *p++ = 0xc0;

        // ret
        *p++ = 0xc3;
    }

    /* Output */
    else if (__builtin_expect(opcode == 10, 0))
    {
        void *putchar_addr = (void *)&putchar;

        // mov edi, rXd (where X is reg_num)
        *p++ = 0x44; // Reg prefix for r8-r15
        *p++ = 0x89;
        *p++ = 0xc7 | (c << 3); // ModR/M byte: edi(111) with reg number

        // push r8 - r11 onto the stack
        *p++ = 0x41;
        *p++ = 0x50;

        *p++ = 0x41;
        *p++ = 0x51;

        // *p++ = 0x41;
        // *p++ = 0x52;

        // *p++ = 0x41;
        // *p++ = 0x53;

        // 12 byte function call;
        *p++ = 0x48; // REX.W prefix
        *p++ = 0xb8; // mov rax, imm64
        memcpy(p, &putchar_addr, sizeof(putchar_addr));
        p += sizeof(putchar_addr);

        // call rax
        *p++ = 0xff;
        *p++ = 0xd0; // ModR/M byte for call rax

        // pop r8 - r11 off the stack
        // *p++ = 0x41;
        // *p++ = 0x5B;

        // *p++ = 0x41;
        // *p++ = 0x5A;

        *p++ = 0x41;
        *p++ = 0x59;

        *p++ = 0x41;
        *p++ = 0x58;

        // ret
        *p++ = 0xc3;
    }

    /* Input */
    else if (__builtin_expect(opcode == 11, 0))
    {
        void *read_char_addr = (void *)&read_char;

        // push r8 - r11 onto the stack
        *p++ = 0x41;
        *p++ = 0x50;

        *p++ = 0x41;
        *p++ = 0x51;

        // *p++ = 0x41;
        // *p++ = 0x52;

        // *p++ = 0x41;
        // *p++ = 0x53;

        // direct function call
        *p++ = 0x48;
        *p++ = 0xb8;

        memcpy(p, &read_char_addr, sizeof(read_char_addr));
        p += sizeof(read_char_addr);

        *p++ = 0xff;
        *p++ = 0xd0;

        // pop r8 - r11 off the stack
        // *p++ = 0x41;
        // *p++ = 0x5B;

        // *p++ = 0x41;
        // *p++ = 0x5A;

        *p++ = 0x41;
        *p++ = 0x59;

        *p++ = 0x41;
        *p++ = 0x58;

        // mov rCd, eax
        *p++ = 0x41;
        *p++ = 0x89;
        *p++ = 0xC0 | c;

        // ret
        *p++ = 0xc3;
    }

    /* Stop or Invalid Instruction */
    else
    {
        // ret
        *p++ = 0xc3;
        return true;
    }

    return false;
}

unsigned char read_char()
{
    int x = getc(stdin);
    assert(x != EOF);
    unsigned char c = (unsigned char)x;
    return c;
}

// Sandmark doesn't work with this method: To be fixed
void load_segment(uint32_t index, uint32_t *zero)
{
    if (index > 0)
    {
        uint32_t copied_seq_size = segment_lengths[index];
        memcpy(zero, segment_sequence[index], copied_seq_size * sizeof(uint32_t));
    }
}

// Set up functions
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

uint32_t *initialize_memory(FILE *fp, size_t num_words)
{
    seq_capacity = 128;
    segment_sequence = (uint32_t **)calloc(seq_capacity, sizeof(uint32_t *));
    segment_lengths = (uint32_t *)calloc(seq_capacity, sizeof(uint32_t));

    rec_capacity = 128;
    recycled_ids = (uint32_t *)calloc(rec_capacity, sizeof(uint32_t *));

    /* Load initial segment from file */
    uint32_t *zero = (uint32_t *)calloc(num_words, sizeof(uint32_t));
    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;

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
            zero[i / 4] = word;
            word = 0;
        }
        i++;
    }

    fclose(fp);
    segment_sequence[0] = zero;
    segment_lengths[0] = num_words;
    seq_size++;

    return zero;
}

// Clean up:
void handle_stop()
{
    for (uint32_t i = 0; i < seq_size; i++)
        free(segment_sequence[i]);
    free(segment_sequence);
    free(segment_lengths);
    free(recycled_ids);
}

uint32_t map_segment(uint32_t size)
{
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (rec_size == 0)
    {
        if (seq_size == seq_capacity)
        {
            /* Expand the sequence if necessary */
            seq_capacity = seq_capacity * 2 + 2;
            segment_lengths = (uint32_t *)realloc(segment_lengths,
                                                  (seq_capacity) * sizeof(uint32_t));
            segment_sequence = (uint32_t **)realloc(segment_sequence,
                                                    (seq_capacity) * sizeof(uint32_t *));

            for (uint32_t i = seq_size; i < seq_capacity; i++)
            {
                segment_sequence[i] = NULL;
                segment_lengths[i] = 0;
            }
        }

        new_seg_id = seq_size++;
    }

    /* Otherwise, reuse an old one */
    else
        new_seg_id = recycled_ids[--rec_size];

    if (segment_sequence[new_seg_id] == NULL ||
        size > segment_lengths[new_seg_id])
    {
        segment_sequence[new_seg_id] =
            (uint32_t *)realloc(segment_sequence[new_seg_id],
                                size * sizeof(uint32_t));
        segment_lengths[new_seg_id] = size;
    }

    /* Zero out the segment */
    memset(segment_sequence[new_seg_id], 0, size * sizeof(uint32_t));
    return new_seg_id;
}

void unmap_segment(uint32_t segment)
{
    if (rec_size == rec_capacity)
    {
        rec_capacity = rec_capacity * 2 + 2;
        recycled_ids = (uint32_t *)realloc(recycled_ids, (rec_capacity) * sizeof(uint32_t));
    }

    recycled_ids[rec_size++] = segment;
}


uint32_t segmented_load(uint32_t b_val, uint32_t c_val)
{
    /* For a segmented load, we only get the relevant value from the value
     * segment, since it makes no sense to put compiled instructions in a
     * register. This value can be compiled when it comes time to put it back
     * into a memory segment */

    /* The return value gets loaded into the correct register by the calling
     * assembly */
    // assert(gs.val_seq[b_val]);
    // assert(c_val < gs.seg_lens[b_val]);

    // uint32_t x = gs.val_seq[b_val][c_val];
    uint32_t x = segment_sequence[b_val][c_val];
    // printf("The seg loaded word is %u\n", x);
    return x;
}

void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val)
{
    /* Load the inputted word into value memory */

    // gs.val_seq[a_val][b_val] = c_val;
    segment_sequence[a_val][b_val] = c_val;

}

// This function is important because it is at the hybrid of JIT and emulator

// void *load_program(uint32_t b_val, uint32_t c_val)
void load_program(uint32_t b_val, uint32_t c_val)
{
    /* Set the program counter to be the contents of register c */
    // gs.pc = c_val;
    pc = c_val;

    if (b_val == 0)
    {
        // return segment_sequence[0];
        return;
    }

    // TODO: Free the existing zero segment

    uint32_t new_seg_size = segment_lengths[b_val];

    uint32_t *new_vals = calloc(new_seg_size, sizeof(uint32_t));
    memcpy(new_vals, segment_sequence[b_val], new_seg_size * sizeof(uint32_t));

    /* Update the existing memory segment */
    // gs.val_seq[0] = new_vals;

    segment_sequence[0] = new_vals;

    segment_lengths[0] = new_seg_size;

    return;
    // return new_zero;
}