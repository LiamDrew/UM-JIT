/*
 * TODOs at this point:
 * 1. Do more cleanup and polishing of the Stock UM so that it can be a shiny
 *    example of a great starting point
 * 2. Find out what my options are for profiling (if any)
 * 3. Get more low hanging fruit off this simple version
 * 4. Ask ChatGPT what it can do for me.
 * 5. Figure out how to determine the size of a file more easily (that is a quick boost in speed)
 * 6. Consider more aggressive design solutions for optimizing the program (Is sequencing the best way to do things?)
 * 7. After all of the above is rock solid, start thinking about x86 assembly. This of course will require a lot of setup
 *    Either I have to put the project back on the department servers or I have to use a docker image or some VM
 *    Basically I have to figure out how to run this program on Linux, umdump it, and then begin the slow and tedious process
 *    of figuring how to do all this shit in assembly. I think this will be a monumental pain in the ass.
 * 8. If by the grace of god I figure out how to do this for x86, the final boss will be figuring out how to do this
 *    locally on my ARM machine. This is why the fully functional natively implemented program is so important: I have to have
 *    control over every last byte with the asssembly, so I have to take extreme ownership of everything.

 */

/* More ideas:
 * Look at the profiling assignment for more ideas of how to get the low hanging fruit
 * The segments should be in global memory. The registers should be in global memory.
 * Functions should be static and inline, if they are even separated at all.
 * 
 * Also, don't neglect the IO module again. That was what cost us Excellent on everything for the profiling assignments.
 * */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define NUM_REGISTERS 8

// DATA STRUCTURES

// Array
typedef struct {
    unsigned size;
    unsigned capacity;
    uint32_t *buckets;
} Array_T;

Array_T *new_array(unsigned capacity);
void free_array(Array_T **arr);
uint32_t array_at(Array_T *arr, unsigned idx);
void array_update(Array_T *arr, unsigned idx, uint32_t word);
Array_T *array_copy(Array_T *arr, unsigned length);

// TODO: one of these is not necessary, I'm forgetting which right now
unsigned array_capacity(Array_T *arr);
unsigned array_size(Array_T *arr);

// Sequence
typedef struct {
    uint32_t size;
    uint32_t capacity;
    void **data;
} Sequence_T;

Sequence_T *Seq_new();
void Seq_free(Sequence_T **seq);
void Seq_addlo(Sequence_T *seq, void *ptr);
void Seq_addhi(Sequence_T *seq, void *ptr);
void *Seq_remlo(Sequence_T *seq);
void *Seq_remhi(Sequence_T *seq);
void *Seq_get(Sequence_T *seq, uint32_t idx);
void Seq_put(Sequence_T *seq, uint32_t idx, void *ptr);
uint32_t Seq_length(Sequence_T *seq);



// MEMORY

typedef struct {
    Sequence_T *segment_sequence;
    Sequence_T *recycled_ids;
    uint32_t *registers;
} Memory_T;

typedef uint32_t Instruction;
Instruction fetch_instruction(Memory_T *mem, int program_counter);

Memory_T *initialize_memory(FILE *fp);
Array_T *load_initial_segment(FILE *fp);
uint32_t get_register(Memory_T *mem, unsigned index);
void set_register(Memory_T *mem, unsigned index, uint32_t new_contents);

uint32_t segmented_load(Memory_T *mem, uint32_t segment, uint32_t index);
void segmented_store(Memory_T *mem, uint32_t segment, uint32_t index,
                     uint32_t value);

uint32_t map_segment(Memory_T *mem, uint32_t size, bool *mem_exhausted);
void unmap_segment(Memory_T *mem, uint32_t segment);

void load_program(Memory_T *mem, uint32_t segment);
void handle_halt(Memory_T *program_memory);
void print_registers(Memory_T *mem);



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

void printBinary(uint64_t num, char *name);



// CONTROLLERS

void start_um(FILE *fp);
void processor_cycle(u_int32_t *ctr, Memory_T *curr_memory);
int64_t handle_instruction(Memory_T *mem, Instruction word);

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

    start_um(fp);
    return EXIT_SUCCESS;
}

void start_um(FILE *fp)
{
    // Set program counter to be 0
    u_int32_t *ctr = malloc(sizeof(uint32_t));
    assert(ctr != NULL);
    *ctr = 0;

    // Initialize machine memory
    Memory_T *mem = initialize_memory(fp);

    // Start instruction loop
    processor_cycle(ctr, mem);

    // TODO: Free machine memory when done
    free(ctr);
    free(mem);
}

// takes prgram data and current memory
void processor_cycle(u_int32_t *ctr, Memory_T *curr_memory)
{
    bool keep_running = true;
    while (keep_running)
    {
        /* Fetching next instruction from memory */
        Instruction next_instruction = fetch_instruction(curr_memory, *ctr);

        /* Handling fetched instruction */
        int exit_code = handle_instruction(curr_memory, next_instruction);

        if (exit_code == -2)
        {
            keep_running = false;
            return;
        }

        else if (exit_code >= 0)
        { /* Set program counter to LV*/
            *ctr = (uint32_t)exit_code;
        }

        else
        { /* Incrementing Program Counter*/
            *ctr += 1;
        }
    }
}

int64_t handle_instruction(Memory_T *mem, Instruction word)
{
    /* decode the opcode, a, b, c*/
    unsigned int a = 0, b = 0, c = 0;
    uint32_t value_to_load = 0;
    unsigned int opcode = Bitpack_getu(word, 4, 28);
    if (opcode == 13)
    {
        /* Load value into $r[A] */
        a = Bitpack_getu(word, 3, 25);
        value_to_load = Bitpack_getu(word, 25, 0);
    }

    else
    {
        c = Bitpack_getu(word, 3, 0);
        b = Bitpack_getu(word, 3, 3);
        a = Bitpack_getu(word, 3, 6);
    }

    /* power stores 2^32 for use with arithmetic operations */
    uint64_t power = (uint64_t)1 << 32;

    /* temp is used in some arithmetic operations to prevent integer
     * overflow when adding and multiplying uint32_ts */

    uint64_t temp;
    /* In this switch table, we handle every single UM instruction */
    bool mem_exhausted = false;

    switch (opcode)
    {
    case 0:
        /* Conditional Move */
        // This could be rewritten at some point, but now's not the time
        if (get_register(mem, c) == 0)
        {
            break;
        }
        else
        {
            set_register(mem, a, get_register(mem, b));
        }

        break;
    case 1:
        /* Segmented Load (Memory Module) */
        set_register(mem, a, segmented_load(mem, get_register(mem, b), get_register(mem, c)));
        break;
    case 2:
        /* Segmented Store (Memory Module) */
        segmented_store(mem, get_register(mem, a),
                        get_register(mem, b), get_register(mem, c));
        break;
    case 3:
        /* Addition */
        temp = get_register(mem, b);
        temp += get_register(mem, c);
        set_register(mem, a, (temp % power));
        break;
    case 4:
        /* Multiplication */
        temp = get_register(mem, b);
        temp *= get_register(mem, c);
        set_register(mem, a, (temp % power));
        break;
    case 5:
        /* Division */
        temp = get_register(mem, b);
        temp = (temp / get_register(mem, c));
        set_register(mem, a, temp);
        break;
    case 6:
        /* Bitwise NAND */
        set_register(mem, a,
                     ~(get_register(mem, b) & get_register(mem, c)));
        break;
    case 7:
        /* Halt */
        handle_halt(mem);
        /* This exit code of -2 tells the operations module to
         * stop the processor cycle */
        return -2;
        break;
    case 8:
        /* Map Segment (Memory Module) */
        // printf("Mapping segment\n");

        set_register(mem, b, map_segment(mem, get_register(mem, c), &mem_exhausted));
        if (mem_exhausted)
        {
            printf("Mem has been exhausted\n");
            handle_halt(mem);
            return -2;
        }
        break;
    case 9:

        // printf("UNMAPping segment\n");
        /* Unmap Segment (Memory Module) */
        unmap_segment(mem, get_register(mem, c));
        break;
    case 10:
        /* Output (IO) */
        output_register(get_register(mem, c));
        break;
    case 11:
        /* Input (IO) */
        set_register(mem, c, read_in_to_register());
        break;
    case 12:
        // printf("Loading the program\n");
        /* Load Program (Memory Module) */
        load_program(mem, get_register(mem, b));
        /* This exit code returns the new value of the program
         * counter */
        return (int64_t)get_register(mem, c);
    case 13:
        /* Load Value */
        set_register(mem, a, value_to_load);
        break;
    }

    /* This exit code of -1 means everything is normal */
    return -1;
}

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
    if (c == EOF)
    {
        contents -= 1;
        // fprintf(stderr, "contents is %u \n", contents);
        return contents;
    }
    else
    {
        assert(c <= 255);
        contents = (uint32_t)c;
        return contents;
    }
}

Instruction fetch_instruction(Memory_T *curr_memory, int program_counter)
{
    /* Get segment 0 */
    Array_T *segment_zero = (Array_T *)Seq_get(curr_memory->segment_sequence, 0);

    if (segment_zero == NULL)
    {
        printf("Seq size is %d\n", curr_memory->segment_sequence->size);
    }
    // The zero segment CANNOT be null
    assert(segment_zero != NULL);

    /* get instruction*/
    Instruction instr = array_at(segment_zero, program_counter);
    return instr;
}

Memory_T *initialize_memory(FILE *fp)
{
    // printf("Loading initial segment");
    // load initial segment
    Array_T *in_seg = load_initial_segment(fp);

    // make new UM memory
    Memory_T *memory = malloc(sizeof(Memory_T));
    assert(memory != NULL);

    // Make new sequences to keep track of new and recycled segments
    memory->segment_sequence = Seq_new();
    memory->recycled_ids = Seq_new();

    // printf("adding segment\n");

    // add the initially loaded segment to the start of the new segment sequence
    Seq_addlo(memory->segment_sequence, in_seg);
    printf("Only happening once\n");

    // printf("Init registers\n");

    // initialize all registers
    memory->registers = calloc(NUM_REGISTERS, sizeof(uint32_t));
    assert(memory->registers != NULL);

    return memory;
}

Array_T *load_initial_segment(FILE *fp)
{
    assert(fp != NULL);

    // get the size of the file
    // TODO: i can do this more efficiently
    fseek(fp, 0L, SEEK_END);
    int file_size = ftell(fp);
    rewind(fp);

    // Make new array for initial segment
    // printf("Array size is going to be %d\n", file_size);
    Array_T *arr = new_array(file_size);
    uint32_t word = 0;

    int c;
    int i = 0;
    unsigned char c_char;

    for (c = getc(fp); c != EOF; c = getc(fp))
    {
        c_char = (unsigned char)c;

        /* Each word is made up of 4 chars. Here we build the word one
         * char at a time in Big Endian Order */

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
            // TODO: actually put the bitpacked word into the program.
            array_update(arr, (i / 4), word);
            // arr->size++; // TODO: quick and dirty fix;
            word = 0;
        }
        i++;
    }

    fclose(fp);
    return arr;
}

uint32_t get_register(Memory_T *mem, unsigned index)
{
    assert(mem);
    assert(index < 8);
    return mem->registers[index];
}

void set_register(Memory_T *mem, unsigned index, uint32_t new_contents)
{
    assert(mem);
    assert(index < 8);
    mem->registers[index] = new_contents;
}

// I seriously need to improve the way I handle the mapping and unmapping of segments

uint32_t map_segment(Memory_T *mem, uint32_t size, bool *mem_exhausted)
{
    // printf("mapping a seq\n");
    assert(mem != NULL);
    Array_T *new_seg = new_array(size);

    /* Sets every word in the segment equal to 0 */
    for (uint32_t i = 0; i < size; i++)
    {
        array_update(new_seg, i, 0);
        // new_seg->size++; // cheap and bad fix
    }

    uint32_t new_seg_id;
    uint32_t hi = 0;
    hi -= 1;
    int avail_ids = Seq_length(mem->recycled_ids);

    /* If there are no available recycled segment ids, make a new one */
    if (avail_ids == 0)
    {
        /* Check and make sure that the UM hasn't run out of segments */
        new_seg_id = Seq_length(mem->segment_sequence);
        if (new_seg_id == hi)
        {
            assert(false); // big problem
            // RAISE(Exhausted_Segs);
            // UArray_free(&new_seg);
            *mem_exhausted = true;
            return 0;
        }
        Seq_addhi(mem->segment_sequence, new_seg);
    }

    /* Otherwise, reuse an old one */
    else
    {
        new_seg_id = (uint32_t)(uintptr_t)Seq_remlo(mem->recycled_ids);
        Seq_put(mem->segment_sequence, new_seg_id, new_seg);
    }

    return new_seg_id;
}

void unmap_segment(Memory_T *mem, uint32_t segment)
{
    assert(mem != NULL);

    // cannot allow the program to umap the 0 segment
    assert(segment != 0);

    // // get the segment we are recycling by segment_ID
    Array_T *to_be_freed = (Array_T *)Seq_get(mem->segment_sequence, segment);
    assert(to_be_freed != NULL);

    // printf("Unmapping Segment number is %u\n", segment);

    // /* Adding the segment identifier of the newly unmapped segment to the
    //  * recycling sequence */
    Seq_addhi(mem->recycled_ids, (void *)(uintptr_t)segment);
    // /* Setting the pointer to the unmapped sequence to NULL to avoid
    //  * double frees */

    // printf("Unmapping Segment number is %u\n", segment);

    // // TODO: we have a major bug here. This is setting the active segment to null.
    Seq_put(mem->segment_sequence, segment, NULL);
    free_array(&to_be_freed);
}

uint32_t segmented_load(Memory_T *mem, uint32_t segment, uint32_t index)
{
    assert(mem);
    Array_T *get_segment = (Array_T *)Seq_get(mem->segment_sequence,
                                              segment);
    assert(get_segment);

    return array_at(get_segment, index);
}

// something may not be quite right here. I think I know what it is
void segmented_store(Memory_T *mem, uint32_t segment, uint32_t index,
                     uint32_t value)
{
    assert(mem);
    Array_T *get_segment = (Array_T *)Seq_get(mem->segment_sequence,
                                              segment);
    assert(get_segment);
    array_update(get_segment, index, value);
}

void load_program(Memory_T *mem, uint32_t segment)
{
    assert(mem);
    assert(mem->segment_sequence);

    if (segment == 0)
    {
        return;
    }

    Array_T *get_segment = (Array_T *)Seq_get(mem->segment_sequence,
                                              segment);

    // try this
    Array_T *first_segment = array_copy(get_segment,
                                        array_capacity(get_segment));

    // this absolutely has to work
    assert(first_segment != NULL);
    Seq_put(mem->segment_sequence, 0, first_segment);
}

void handle_halt(Memory_T *program_memory)
{
    free(program_memory->registers);
    int seq_len = Seq_length(program_memory->segment_sequence);

    for (int i = 0; i < seq_len; i++)
    {
        Array_T *to_be_freed = (Array_T *)
            Seq_get(program_memory->segment_sequence, i);
        /* Ensuring that no NULL pointers get freed */
        if (to_be_freed != NULL)
        {
            free_array(&to_be_freed);
        }
    }

    Seq_free(&(program_memory->segment_sequence));
    Seq_free(&(program_memory->recycled_ids));
}

void print_registers(Memory_T *mem)
{
    uint32_t r0 = get_register(mem, 0);
    uint32_t r1 = get_register(mem, 1);
    uint32_t r2 = get_register(mem, 2);
    uint32_t r3 = get_register(mem, 3);
    uint32_t r4 = get_register(mem, 4);
    uint32_t r5 = get_register(mem, 5);
    uint32_t r6 = get_register(mem, 6);
    uint32_t r7 = get_register(mem, 7);
    fprintf(stderr,
            "REG: 0=%d, 1=%d, 2=%d, 3=%d, 4=%d, 5=%d, 6=%d, 7=%d\n",
            r0, r1, r2, r3, r4, r5, r6, r7);
}

// starting capacity of 10
Sequence_T *Seq_new()
{
    Sequence_T *seq = malloc(sizeof(Sequence_T));
    assert(seq != NULL);

    seq->size = 0;
    seq->capacity = 10;

    seq->data = calloc(seq->capacity, sizeof(void *));
    assert(seq->data != NULL);

    return seq;
}

void Seq_free(Sequence_T **seq)
{
    assert(seq != NULL && *seq != NULL);

    // Leaving this out for now, since it's guaranteed to cause problems
    // for (uint32_t i = 0; i < (*seq)->size; i++) {
    //     // Free everything stored in the sequence
    //     free((*seq)->data[i]);
    //     (*seq)->data[i] = NULL;
    // }

    free((*seq)->data);
    (*seq)->data = NULL;

    free(*seq);
    *seq = NULL;
}

// 100% need a working expand function this time around
void expand(Sequence_T *seq)
{
    assert(seq != NULL);
    assert(seq->size == seq->capacity);

    seq->capacity *= 2;
    void **new_data = calloc(seq->capacity, sizeof(void *));

    for (u_int32_t i = 0; i < seq->size; i++)
    {
        new_data[i] = seq->data[i];
    }

    free(seq->data);
    seq->data = new_data;
}

void Seq_addlo(Sequence_T *seq, void *ptr)
{
    assert(seq != NULL);

    if (seq->size == seq->capacity)
    {
        expand(seq);
    }

    // copy over everything
    for (uint32_t i = seq->size; i > 0; i--)
    {
        seq->data[i] = seq->data[i - 1];
    }

    seq->data[0] = ptr;
    seq->size++;
}

void Seq_addhi(Sequence_T *seq, void *ptr)
{
    assert(seq != NULL);

    if (seq->size == seq->capacity)
    {
        expand(seq);
    }

    seq->data[seq->size] = ptr;
    seq->size++;
}

void *Seq_remlo(Sequence_T *seq)
{
    assert(seq != NULL);
    assert(seq->size > 0);

    void *temp = seq->data[0];

    // TODO: be careful copy over all the elements
    for (uint32_t i = 0; i < seq->size - 1; i++)
    {
        seq->data[i] = seq->data[i + 1];
    }

    // Manually resetting the last element to avoid out of bounds read
    seq->data[seq->size - 1] = NULL;

    seq->size--;
    return temp;
}

void *Seq_remhi(Sequence_T *seq)
{
    assert(seq != NULL);
    assert(seq->size > 0);
    void *temp = seq->data[seq->size - 1];
    seq->data[seq->size - 1] = NULL;

    seq->size--;
    return temp;
}

void *Seq_get(Sequence_T *seq, uint32_t idx)
{
    assert(seq != NULL);
    return seq->data[idx];
}

void Seq_put(Sequence_T *seq, uint32_t idx, void *ptr)
{
    assert(seq != NULL);
    assert(idx < seq->size);

    // This memory needs to get freed somewhere. Probably not in here tho
    seq->data[idx] = ptr;
}

uint32_t Seq_length(Sequence_T *seq)
{
    assert(seq != NULL);
    return seq->size;
}


Array_T *new_array(unsigned capacity)
{
    Array_T *arr = malloc(sizeof(Array_T));
    assert(arr != NULL);

    // allowing unrestricted access to the array, all the way up to capacity
    // arr->size = capacity;

    arr->capacity = capacity;

    arr->buckets = calloc(capacity, sizeof(uint32_t));
    assert(arr->buckets != NULL);

    return arr;
}

void free_array(Array_T **arr)
{
    // NOTE: If the array really contained pointers, we would go through and free them
    free((*arr)->buckets);
    (*arr)->buckets = NULL;

    free(*arr);
    *arr = NULL;
}

void array_expand(Array_T *arr)
{
    // will expand the array;
    (void)arr;
    assert(false);
}

void array_update(Array_T *arr, unsigned idx, uint32_t word)
{
    assert(arr != NULL);
    // printf("Max size is %d, looking at index %d\n", arr->size, idx);
    if (idx == (arr->size + 1))
    {
        // gotcha here bud
        // printf("I neglected something important\n");
        // assert(false);
    }
    // assert(0 <= idx && idx <= arr->size);
    assert(0 <= idx && idx <= arr->capacity);

    arr->buckets[idx] = word;
}

uint32_t array_at(Array_T *arr, unsigned idx)
{

    assert(arr != NULL);
    assert(0 <= idx && idx <= arr->capacity);

    // assert(0 <= idx && idx <= arr->size);

    return arr->buckets[idx];
}


unsigned array_capacity(Array_T *arr)
{
    return arr->capacity;
}

unsigned array_size(Array_T *arr)
{
    return arr->size;
}

Array_T *array_copy(Array_T *arr, unsigned length)
{
    // the capacity doesn't actually matter. It's an array list, treat it as such
    // TODO: find a more elegant solution for this later
    Array_T *new_arr = new_array(length * 2);

    for (unsigned i = 0; i < length; i++)
    {
        new_arr->buckets[i] = arr->buckets[i];
    }

    new_arr->size = length;
    new_arr->capacity = (length * 2);

    return new_arr;
}
