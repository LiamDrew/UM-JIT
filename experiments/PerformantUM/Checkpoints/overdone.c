#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>

#define NUM_REGISTERS 8
#define POWER ((uint64_t)1 << 32);
#define NUM_INSTRS 1

/* BITPACKING */

uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb);
int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb);

uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb, uint64_t value);
uint64_t Bitpack_news(uint64_t word, unsigned width, unsigned lsb, int64_t value);

bool Bitpack_fitsu(uint64_t n, unsigned width);
bool Bitpack_fitss(int64_t n, unsigned width);

/* MEMORY */

typedef uint32_t Instruction;

// Array of segments
uint32_t **segment_sequence = NULL;
uint32_t seq_size = 0;
uint32_t seq_capacity = 0;

// Corresponding length of each segment
uint32_t *segment_lengths = NULL;

// Array of recycled IDs
uint32_t *recycled_ids = NULL;
uint32_t rec_size = 0;
uint32_t rec_capacity = 0;



/* DRIVERS/CONTROLLERS */

uint32_t *initialize_memory(FILE *fp, size_t fsize);
void handle_instructions(uint32_t *zero_segment, size_t fsize);

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
    else
        assert(false);

    // fprintf(stderr, "File size is %zu\n", fsize);
    uint32_t *zero_segment = initialize_memory(fp, fsize + NUM_INSTRS);
    handle_instructions(zero_segment, fsize + NUM_INSTRS);

    return EXIT_SUCCESS;
}

uint32_t *initialize_memory(FILE *fp, size_t fsize)
{
    seq_capacity = 128;

    segment_sequence = calloc(seq_capacity, sizeof(uint32_t *));

    segment_lengths = calloc(seq_capacity, sizeof(uint32_t));

    rec_capacity = 128;
    recycled_ids = calloc(rec_capacity, sizeof(uint32_t *));

    // load initial segment
    // What if I makde it stack alloc'ed?
    // uint32_t alloc_size = fsize / sizeof(uint32_t);
    uint32_t alloc_size = fsize;
    uint32_t *temp = calloc(alloc_size, sizeof(uint32_t));

    uint32_t word = 0;
    int c;
    int i = 0;
    unsigned char c_char;

    for (c = getc(fp); c != EOF; c = getc(fp))
    {
        c_char = (unsigned char)c;

        if (i % 4 == 0)
        {
            word = Bitpack_newu(word, 8, 24, c_char);
        }
        else if (i % 4 == 1)
        {
            word = Bitpack_newu(word, 8, 16, c_char);
        }
        else if (i % 4 == 2)
        {
            word = Bitpack_newu(word, 8, 8, c_char);
        }
        else if (i % 4 == 3)
        {
            word = Bitpack_newu(word, 8, 0, c_char);
            temp[i / 4] = word;
            word = 0;
        }

        i++;
    }

    fclose(fp);
    segment_sequence[0] = temp;
    segment_lengths[0] = alloc_size; // length is the number of words, not number of bytes
    seq_size++;

    return temp;
}

/* INSTRUCTION FETCHING */

/* SIMPLE INSTRUCTION HANDLERS */
static inline void conditional_move(    uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void segmented_load(      uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void segmented_store(     uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void addition(            uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void multiplication(      uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void division(            uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void bitwise_nand(        uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void handle_halt(         uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void map_segment(         uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void unmap_segment(       uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void print_char(          uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void read_char(           uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void load_segment(        uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);
static inline void load_value(          uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val);

static inline uint32_t decode_instruction(uint32_t word, uint32_t *a, uint32_t *b,
                                          uint32_t *c, uint32_t *value_to_load)
{
    uint32_t opcode = (word >> 28) & 0xF;

    if (opcode == 13)
    {
        *a = (word >> 25) & 0x7;
        *value_to_load = word & 0x1FFFFFF;
    }
    else
    {
        *c = word & 0x7;
        *b = (word >> 3) & 0x7;
        *a = (word >> 6) & 0x7;
    }

    return opcode;
}

// static inline void get_val_to_load(uint32_t word, uint32_t *a, uint32_t *val)
// {
//     *a = (word >> 25) & 0x7;
//     *val = word & 0x1FFFFFF;
// }

// static inline void get_registers(u_int32_t word, uint32_t *a, uint32_t *b, uint32_t *c)
// {
//     *c = word & 0x7;
//     *b = (word >> 3) & 0x7;
//     *a = (word >> 6) & 0x7;
// }

// static inline uint32_t get_opcode(u_int32_t word, uint32_t *a, uint32_t *b, uint32_t *c)
// {
//     uint32_t opcode = (word >> 28) & 0xF;

//     *c = word & 0x7;
//     *b = (word >> 3) & 0x7;
//     *a = (word >> 6) & 0x7;

//     return opcode;
// }

// static inline uint32_t get_opcode(uint32_t word)
// {
//     uint32_t opcode = (word >> 28) & 0xF;
//     return opcode;
// }

static inline void do_switch(uint32_t opcode, uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    switch (opcode)
    {

    case 1:
        /* Segmented Load (Memory Module) */
        // get_registers(word, &a, &b, &c);
        segmented_load(a, b, c, regs, zero, pp, val);
        break;
    case 2:
        /* Segmented Store (Memory Module) */
        // get_registers(word, &a, &b, &c);
        segmented_store(a, b, c, regs, zero, pp, val);
        break;
    case 13:
        /* Load Value */
        // get_registers(word, &a, &b, &c);
        // get_val_to_load(word, &a, &val);
        load_value(a, b, c, regs, zero, pp, val);
        break;
    case 10:
        /* Output (IO) */
        // get_registers(word, &a, &b, &c);
        print_char(a, b, c, regs, zero, pp, val);
        break;
    case 0:
        /* Conditional Move */
        // get_registers(word, &a, &b, &c);
        conditional_move(a, b, c, regs, zero, pp, val);
        break;
    case 7:
        /* Halt */
        // get_registers(word, &a, &b, &c);
        handle_halt(a, b, c, regs, zero, pp, val);
        break;
    case 8:
        /* Map Segment (Memory Module) */
        // get_registers(word, &a, &b, &c);
        map_segment(a, b, c, regs, zero, pp, val);
        break;
    case 11:
        /* Input (IO) */
        // get_registers(word, &a, &b, &c);
        read_char(a, b, c, regs, zero, pp, val);
        break;
    case 12:
        /* Load Segment (Memory Module) */
        // get_registers(word, &a, &b, &c);
        load_segment(a, b, c, regs, zero, pp, val);
        break;
    case 6:
        /* Bitwise NAND */
        // get_registers(word, &a, &b, &c);
        bitwise_nand(a, b, c, regs, zero, pp, val);
        break;
    case 3:
        /* Addition */
        // get_registers(word, &a, &b, &c);
        addition(a, b, c, regs, zero, pp, val);
        break;
    case 9:
        /* Unmap Segment (Memory Module) */
        // get_registers(word, &a, &b, &c);
        unmap_segment(a, b, c, regs, zero, pp, val);
        break;
    case 5:
        /* Division */
        // get_registers(word, &a, &b, &c);
        division(a, b, c, regs, zero, pp, val);
        break;
    case 4:
        /* Multiplication */
        // get_registers(word, &a, &b, &c);
        multiplication(a, b, c, regs, zero, pp, val);
        break;

    }
}

// static inline void fill_cache(Instruction *cache, Instruction **program_pointer)
// {
//     memcpy(cache, *program_pointer, NUM_INSTRS * sizeof(Instruction));
//     *program_pointer += NUM_INSTRS;
// }

// static inline Instruction get_instr(Instruction *cache, unsigned index)
// {
//     return cache[index];
// }

static inline Instruction get_instr(Instruction *pp)
{
    return *pp;
}


void handle_instructions(uint32_t *zero, size_t fsize)
{
    uint32_t regs[NUM_REGISTERS] = {0};
    Instruction word;

    uint32_t a = 0, b = 0, c = 0, val = 0;

    for (Instruction *pp = zero; pp < (zero + fsize);) {

        word = get_instr(pp);
        pp++;

        // Consider not decoding every single instruction? It's already super fast though
        uint32_t opcode = decode_instruction(word, &a, &b, &c, &val);
        do_switch(opcode, a, b, c, regs, zero, &pp, val);

    }
}

static inline void load_segment(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)a;
    (void)val;
    uint32_t index = regs[b];
    *pp = zero + regs[c];

    if (index == 0)
        return;

    /* get word size of segment at the provided address */
    uint32_t copied_seq_size = segment_lengths[index];

    // copy bytes over into 0 segment
    memcpy(zero, segment_sequence[index], copied_seq_size * sizeof(uint32_t));

    *pp = zero + regs[c];
}

static inline void map_segment(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)a;
    (void)zero;
    (void)pp;
    (void)val;

    uint32_t size = regs[c];
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (rec_size == 0)
    {
        if (seq_size == seq_capacity)
        { // expand if necessary
            seq_capacity = seq_capacity * 2 + 2;
            segment_sequence = realloc(segment_sequence, (seq_capacity) * sizeof(uint32_t *));
            // set all the new IDs to NULL
            for (uint32_t i = seq_size; i < seq_capacity; i++)
                segment_sequence[i] = NULL;

            segment_lengths = realloc(segment_lengths, (seq_capacity) * sizeof(uint32_t));
            memset(segment_lengths + seq_size, 0, (seq_capacity - seq_size) * sizeof(uint32_t));
        }

        new_seg_id = seq_size++;
    }

    /* Otherwise, reuse an old one */
    else
    {
        rec_size--;
        new_seg_id = recycled_ids[rec_size];
    }

    assert(new_seg_id != 0);

    // If the segment doesn't exist, create it
    if (segment_sequence[new_seg_id] == NULL)
    {
        segment_sequence[new_seg_id] = malloc(size * sizeof(uint32_t));
        segment_lengths[new_seg_id] = size;
    }

    // If the segment is too small, expand it
    else if (size > segment_lengths[new_seg_id])
    {
        segment_sequence[new_seg_id] = realloc(segment_sequence[new_seg_id], size * sizeof(uint32_t));
        segment_lengths[new_seg_id] = size;
    }

    if (segment_sequence[new_seg_id] == NULL || size > segment_lengths[new_seg_id]) {
        segment_sequence[new_seg_id] = realloc(segment_sequence[new_seg_id], size * sizeof(uint32_t));
        segment_lengths[new_seg_id] = size;
    }

    // Update the segment length and zero out memory
    // NOTE: we do not want to reset the size unless we have to
    memset(segment_sequence[new_seg_id], 0, size * sizeof(uint32_t));

    regs[b] = new_seg_id;
}

static inline void unmap_segment(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)a;
    (void)b;
    (void)zero;
    (void)pp;
    (void)val;

    uint32_t segment = regs[c];
    assert(segment != 0);

    if (rec_size == rec_capacity)
    {
        rec_capacity = rec_capacity * 2 + 2;
        recycled_ids = realloc(recycled_ids, (rec_capacity) * sizeof(uint32_t));
    }

    recycled_ids[rec_size++] = segment;
}

/* Needs to free memory */
static inline void handle_halt(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)a;
    (void)b;
    (void)c;
    (void)regs;
    (void)zero;
    (void)pp;
    (void)val;

    for (uint32_t i = 0; i < seq_size; i++)
        free(segment_sequence[i]);

    free(segment_sequence);
    free(segment_lengths);
    free(recycled_ids);

    exit(EXIT_SUCCESS);
}

static inline void conditional_move(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    if (regs[c] != 0)
        regs[a] = regs[b];
}

static inline void segmented_load(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    regs[a] = segment_sequence[regs[b]][regs[c]];
}

static inline void segmented_store(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    segment_sequence[regs[a]][regs[b]] = regs[c];
}

static inline void addition(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    regs[a] = (regs[b] + regs[c]) % POWER;
}

static inline void multiplication(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    regs[a] = (regs[b] * regs[c]) % POWER;
}

static inline void division(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    regs[a] = regs[b] / regs[c];
}

static inline void bitwise_nand(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)zero;
    (void)pp;
    (void)val;

    regs[a] = ~(regs[b] & regs[c]);
}

static inline void print_char(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)a;
    (void)b;
    (void)zero;
    (void)pp;
    (void)val;

    putchar((unsigned char)regs[c]);
}

static inline void read_char(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)a;
    (void)b;
    (void)zero;
    (void)pp;
    (void)val;

    regs[c] = getc(stdin);
}

static inline void load_value(uint32_t a, uint32_t b, uint32_t c, uint32_t *regs, uint32_t *zero, Instruction **pp, uint32_t val)
{
    (void)b;
    (void)c;
    (void)zero;
    (void)pp;

    regs[a] = val;
}



/* BITPACK IMPLEMENTATION */

bool Bitpack_fitsu(uint64_t n, unsigned width)
{
    assert(width <= 64);

    /* If field width is 0, we return true if n is 0, and false otherwise, per
     * page 11 of the spec.
     */

    if (width == 0)
    {
        return (n == 0);
    }

    /* Splitting up the shifting to account for the 64 bit shift edge case */
    uint64_t size = (uint64_t)1 << (width - 1);
    size = size << 1;
    size -= 1;

    return (size >= n);
}

bool Bitpack_fitss(int64_t n, unsigned width)
{
    assert(width <= 64);

    /* Same logic as the previous function */
    if (width == 0)
    {
        return (n == 0);
    }

    /* 64 bit shift is not possible here */
    int64_t magnitude = (int64_t)1 << (width - 1);

    /* Determining the bounds of n based on 2's compliment */
    int64_t upper_bound = magnitude - 1;
    int64_t lower_bound = -1 * magnitude;

    return (n >= lower_bound && n <= upper_bound);
}

uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb)
{
    /* Asserting necessary conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    /* Fields of width 0 contain value 0 */
    if (width == 0)
    {
        return 0;
    }

    word = word >> lsb;
    uint64_t mask = 0;

    /* Returns the part of the word we care about */
    mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;

    return (word & mask);
}

int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb)
{
    /* Asserting conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    /* Fields of 0 contain value 0 */
    if (width == 0)
    {
        return 0;
    }

    word = word >> lsb;
    uint64_t mask = 0;
    mask -= 1;

    /* Splitting up mask shifting */
    mask = mask << (width - 1);
    mask = mask << 1;
    mask = ~mask;

    /* Returns the bits of interest */
    int64_t new_int = (word & mask);

    /* Shifts and shifts back in order to propagate the signed bit */
    new_int = new_int << (64 - width);
    new_int = new_int >> (64 - width);

    return new_int;
}

uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb, uint64_t value)
{
    /* Asserting Conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    /* Check to make sure value can fit in number of bits */
    if (!Bitpack_fitsu(value, width))
    {
        assert(false);
    }

    /* If value and width are 0 and lsb is 64, return word */
    if (width == 0)
    {
        return word;
    }

    uint64_t mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;

    /* There is no way for lsb to be 64 here, since either an earlier assert
     * would have failed or the function would have returned 0 */
    mask = mask << lsb;
    mask = ~mask;

    uint64_t new_word = (word & mask);

    value = value << lsb;
    uint64_t return_word = (new_word | value);

    return return_word;
}

uint64_t Bitpack_news(uint64_t word, unsigned width, unsigned lsb, int64_t value)
{
    /* Asserting Conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    if (!Bitpack_fitss(value, width))
    {
        assert(false);
        return 0;
    }

    if (width == 0)
    {
        return word;
    }

    uint64_t mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;
    mask = mask << lsb;
    mask = ~mask;

    uint64_t new_word = (word & mask);

    value = value << lsb;
    uint64_t cast = (uint64_t)value;

    /* Shifting and reshifting to propagate the signed bit */
    cast = cast << (64 - width - lsb);
    cast = cast >> (64 - width - lsb);

    uint64_t return_word = (new_word | cast);
    return return_word;
}