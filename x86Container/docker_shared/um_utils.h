#ifndef UM_UTILS_H
#define UM_UTILS_H

#include <stdint.h>
#include <stdlib.h>

// may experiement with making it 14, but for now keep it a power of 2
// due to unforseen circumstances, we have to make it a disgusting 32. ugh
// due to more terrible things, the chunk is now 40 and the MULT is 10
#define CHUNK 64
#define MULT (CHUNK / 4)
#define OPS 15

#define AS 1
#define A 6

#define BS 8
#define B 1

#define CS 8
#define C 4

#define ICAP 128

typedef uint32_t Instruction;

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

typedef struct {
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
};


extern struct GlobalState gs;

extern struct MachineCode mc;

void print_memory(const void *memory, size_t size);

void print_out(uint32_t x);
unsigned char read_char(void);

void print_registers();

size_t compile_instruction(void *zero, uint32_t word, size_t offset);
size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value);

size_t print_reg(void *zero, size_t offset, unsigned c);
size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t handle_halt(void *zero, size_t offset);
size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);

size_t read_into_reg(void *zero, size_t offset, unsigned c);
size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c);
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c);
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c);

uint32_t map_segment(uint32_t size);

void handle_realloc();
void unmap_segment(uint32_t segmentID);

uint32_t segmented_load(uint32_t b_val, uint32_t c_val);
void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val);
void *load_program(uint32_t b_val, uint32_t c_val);

#endif