#ifndef MEMORY_H__
#define MEMORY_H__

#include "array.h"
#include "sequence.h"

#include <stdlib.h>
#include <stdio.h>

#include "bitpack.h"
// #include <stdbool.h>

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

#endif