#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>

#define NUM_REGISTERS 8

/* MEMORY */
typedef uint32_t Instruction;

// Initializing program counter and all registers to 0
uint32_t program_counter = 0;
uint32_t registers[NUM_REGISTERS] = {0};

// Array of segments
uint32_t **segment_sequence;
uint32_t seq_size = 0;
uint32_t seq_capacity = 0;

// Corresponding length of each segment
uint32_t *segment_lengths;

// Array of recycled IDs
uint32_t *recycled_ids;
uint32_t rec_size = 0;
uint32_t rec_capacity = 0;

uint64_t power = (uint64_t)1 << 32;

uint32_t map_segment(uint32_t size);
void unmap_segment(uint32_t segment);
void load_program(uint32_t segment);

// IO

void output_register(uint32_t register_contents);
uint32_t read_in_to_register();

// BITPACKING

uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb);
int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb);

uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb, uint64_t value);
uint64_t Bitpack_news(uint64_t word, unsigned width, unsigned lsb, int64_t value);

bool Bitpack_fitsu(uint64_t n, unsigned width);
bool Bitpack_fitss(int64_t n, unsigned width);


// CONTROLLERS

void initialize_memory(FILE *fp, size_t fsize);
void handle_instructions();
void handle_halt(); // NOTE: May need fixing

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
        fprintf(stderr, "File could not be opened.\n");
        return EXIT_FAILURE;
    }


    size_t fsize = 0;
    struct stat file_stat;
    if (stat(argv[1], &file_stat) == 0) {
        fsize = file_stat.st_size;
    } else {
        assert(false);
    }

    initialize_memory(fp, fsize);

    handle_instructions();

    return EXIT_SUCCESS;
}


void initialize_memory(FILE *fp, size_t fsize)
{
    seq_capacity = 128;

    segment_sequence = malloc(seq_capacity * sizeof(uint32_t *));
    assert(segment_sequence != NULL);

    segment_lengths = malloc(seq_capacity * sizeof(uint32_t));
    assert(segment_lengths != NULL);

    rec_capacity = 128;
    recycled_ids = malloc(rec_capacity * sizeof(uint32_t*));
    assert(recycled_ids);

    // load initial segment
    uint32_t *temp = malloc(fsize * sizeof(uint32_t));
    assert(temp != NULL);

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
    segment_lengths[0] = fsize;
    seq_size++;
}

void handle_instructions()
{
    Instruction word;
    while (true) {
        word = segment_sequence[0][program_counter];
        program_counter++;

        uint32_t a, b, c;
        uint32_t value_to_load = 0;
        uint32_t opcode = word;
        opcode = opcode >> 28;

        if (opcode == 13) {
            a = (word >> 25) & 0x7;
            value_to_load = word & 0x1FFFFFF;
        } else {
            c = word & 0x7;
            b = (word >> 3) & 0x7;
            a = (word >> 6) & 0x7;
        }

        switch (opcode) {
            case 0:
                /* Conditional Move */
                if (registers[c] != 0) {
                    registers[a] = registers[b];
                }
                break;
            case 1:
                /* Segmented Load (Memory Module) */
                registers[a] = segment_sequence[registers[b]][registers[c]];

                // registers[a] = segmented_load(registers[b],
                //                             registers[c], segment_sequence);
                break;
            case 2:
                /* Segmented Store (Memory Module) */
                segment_sequence[registers[a]][registers[b]] = registers[c];
                // segmented_store(registers[a],
                //                 registers[b], registers[c], segment_sequence);
                break;
            case 3:
                /* Addition */
                registers[a] = (registers[b] + registers[c]) % power;
                break;
            case 4:
                /* Multiplication */
                registers[a] = (registers[b] * registers[c]) % power;
                break;
            case 5:
                /* Division */
                registers[a] = registers[b] / registers[c];
                break;
            case 6:
                registers[a] = ~(registers[b] & registers[c]);
                break;
            case 7:
                /* Halt */
                handle_halt();
                break;
            case 8:
                /* Map Segment (Memory Module) */
                registers[b] = map_segment(registers[c]);
                break;
            case 9:
                /* Unmap Segment (Memory Module) */
                unmap_segment(registers[c]);
                break;
            case 10:
                /* Output (IO) */
                output_register(registers[c]);
                break;
            case 11:
                /* Input (IO) */
                registers[c] = read_in_to_register();
                break;
            case 12:
                /* Load Program (Memory Module) */
                load_program(registers[b]);
                program_counter = registers[c];
                break;
            case 13:
                /* Load Value */
                registers[a] = value_to_load;
                break;
            }
    }
}

/* Needs to free memory */
void handle_halt()
{
    for (uint32_t i = 0; i < seq_size; i++) {
        if (segment_sequence[i] != NULL) {
            free(segment_sequence[i]);
        }
    }

    free(segment_sequence);
    free(segment_lengths);
    free(recycled_ids);

    // NOTE: I distrust this. Won't this lead to a memory leak?
    exit(EXIT_SUCCESS);
}

void output_register(uint32_t register_contents)
{
    /* Assert it's a char*/
    assert(register_contents <= 255);
    /* Print the char*/
    unsigned char c = (unsigned char)register_contents;
    printf("%c", c);
}

uint32_t read_in_to_register()
{
    int c;
    uint32_t contents = 0;
    c = getc(stdin);
    /* Check it's not an EOF character*/
    if (c == EOF) {
        contents -= 1;
        // fprintf(stderr, "contents is %u \n", contents);
        return contents;
    } else {
        assert(c <= 255);
        contents = (uint32_t)c;
        return contents;
    }
}


// I seriously need to improve the way I handle the mapping and unmapping of segments
uint32_t map_segment(uint32_t size)
{
    uint32_t *temp = calloc(size, sizeof(uint32_t));
    uint32_t new_seg_id;

    /* If there are no available recycled segment ids, make a new one */
    if (rec_size == 0) {
        if (seq_size == seq_capacity) { // expand if necessary
            seq_capacity = seq_capacity * 2 + 2;
            segment_sequence = realloc(segment_sequence, (seq_capacity) * sizeof(uint32_t *));
            segment_lengths = realloc(segment_lengths, (seq_capacity) * sizeof(uint32_t));
        }

        segment_sequence[seq_size] = temp;
        new_seg_id = seq_size;
        seq_size++;
    }

    /* Otherwise, reuse an old one */
    else {
        rec_size--;
        new_seg_id = recycled_ids[rec_size];
        recycled_ids[rec_size] = 0;
        segment_sequence[new_seg_id] = temp;
    }

    segment_lengths[new_seg_id] = size;
    return new_seg_id;
}

void unmap_segment(uint32_t segment)
{
    uint32_t *to_be_freed = segment_sequence[segment];
    free(to_be_freed);
    segment_sequence[segment] = NULL;

    if (rec_size == rec_capacity) {
        rec_capacity = rec_capacity * 2 + 2;
        // realloc recycled ids
        recycled_ids = realloc(recycled_ids, (rec_capacity) * sizeof(uint32_t));
    }
    recycled_ids[rec_size] = segment;
    segment_lengths[segment] = 0;
    rec_size++;
}

void load_program(uint32_t segment)
{

    if (segment == 0) { return; }

    /* get address of segment at provided index*/
    uint32_t copied_seq_size = segment_lengths[segment];

    /* free the first segment */
    free(segment_sequence[0]);

    /* malloc a new segment in index 0 with size of one to be copied */
    segment_sequence[0] = (uint32_t *)malloc(copied_seq_size * sizeof(uint32_t));
    
    /* Cache performance? */
    for (uint32_t i = 0; i < copied_seq_size; i++) {
        segment_sequence[0][i] = segment_sequence[segment][i];
    }

    segment_lengths[0] = copied_seq_size;
}


// BITPACK IMPLEMENTATION

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
        // RAISE(Bitpack_Overflow);
        // Moving everything to native
        assert(false);
        return word;
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