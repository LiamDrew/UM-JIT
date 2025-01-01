#ifndef UM_UTILS_H
#define UM_UTILS_H
#include <stdint.h>
#include <stdlib.h>

#define RO 8

// may experiement with making it 14, but for now keep it a power of 2
#define CHUNK 16
#define MULT 4

typedef uint32_t Instruction;


struct GlobalState
{
    uint32_t pc; // program counter
    void **program_seq;
    uint32_t **val_seq;
    uint32_t *seg_lens;
    uint32_t seq_size;
    uint32_t seq_cap;
    
    uint32_t *rec_ids;
    uint32_t rec_size;
    uint32_t rec_cap;
} global_state;

extern struct GlobalState gs;

void print_registers();

size_t compile_instruction(void *zero, uint32_t word, size_t offset);
size_t load_reg(void *zero, size_t offset, unsigned a, uint32_t value);

void print_out(uint32_t x);
size_t print_reg(void *zero, size_t offset, unsigned reg);
size_t add_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t handle_halt(void *zero, size_t offset);
size_t mult_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t div_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t cond_move(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);
size_t nand_regs(void *zero, size_t offset, unsigned a, unsigned b, unsigned c);

size_t read_into_reg(void *zero, size_t offset, unsigned reg);
size_t inject_map_segment(void *zero, size_t offset, unsigned b, unsigned c);
size_t inject_unmap_segment(void *zero, size_t offset, unsigned c);
size_t inject_seg_load(void *zero, size_t offset, unsigned a, unsigned b, unsigned c, Instruction word);
size_t inject_seg_store(void *zero, size_t offset, unsigned a, unsigned b, unsigned c, Instruction word);
size_t inject_load_program(void *zero, size_t offset, unsigned b, unsigned c);

unsigned char read_char(void);
uint32_t map_segment(uint32_t size);
void unmap_segment(uint32_t segmentID);
uint32_t segmented_load(uint32_t b_val, uint32_t c_val);
void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val);
void *load_program(uint32_t b_val, uint32_t c_val);

#endif