#include "memory.h"
#include "bitpack.h"
#include <assert.h>

#define NUM_REGISTERS 8

Instruction fetch_instruction(Memory_T *curr_memory, int program_counter)
{
    /* Get segment 0 */
    Array_T *segment_zero = (Array_T*) Seq_get(curr_memory->segment_sequence, 0);

    if (segment_zero == NULL) {
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



uint32_t map_segment(Memory_T *mem, uint32_t size, bool *mem_exhausted)
{
    assert(mem != NULL);
    Array_T *new_seg = new_array(size);

    /* Sets every word in the segment equal to 0 */
    for (uint32_t i = 0; i < size; i++)
    {
        array_update(new_seg, i, 0);
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
    Array_T *to_be_freed = (Array_T*) Seq_get(mem->segment_sequence, segment);
    assert(to_be_freed != NULL);

    // printf("Unmapping Segment number is %u\n", segment);

    // /* Adding the segment identifier of the newly unmapped segment to the
    //  * recycling sequence */
    Seq_addhi(mem->recycled_ids, (void *)(uintptr_t)segment);
    // /* Setting the pointer to the unmapped sequence to NULL to avoid
    //  * double frees */

    // printf("Unmapping Segment number is %u\n", segment);
    Seq_put(mem->segment_sequence, segment, NULL);
    free_array(&to_be_freed);
}

uint32_t segmented_load(Memory_T *mem, uint32_t segment, uint32_t index)
{
    assert(mem);
    Array_T *get_segment = (Array_T*)Seq_get(mem->segment_sequence,
                                             segment);
    assert(get_segment);

    return array_at(get_segment, index);
}

// something may not be quite right here. I think I know what it is
void segmented_store(Memory_T *mem, uint32_t segment, uint32_t index,
                     uint32_t value)
{
    assert(mem);
    Array_T *get_segment = (Array_T*)Seq_get(mem->segment_sequence,
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

    Array_T *get_segment = (Array_T*)Seq_get(mem->segment_sequence,
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
        Array_T *to_be_freed = (Array_T*)
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