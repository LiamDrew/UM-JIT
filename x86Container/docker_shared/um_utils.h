#ifndef UM_UTILS_H
#define UM_UTILS_H
#include <stdint.h>

struct GlobalState
{
    void **segment_sequence;
    uint32_t seq_size;
    uint32_t seq_capacity;
    uint32_t *segment_lengths;
    void *recycled_ids;
    uint32_t rec_size;
    uint32_t rec_capacity;
} global_state;

extern struct GlobalState gs;

__attribute__((visibility("default"))) 
unsigned char read_char(void);

__attribute__((visibility("default")))
uint32_t map_segment(uint32_t size);


__attribute__((visibility("default")))
void unmap_segment(uint32_t segmentID);

// /* Ruh roh. This could potentially be another program if this function relies on accessing global memory*/
// uint32_t segmented_load(uint32_t a_val, uint32_t b_val, uint32_t c_val, uint32_t word);
// void segmented_store(uint32_t a_val, uint32_t b_val, uint32_t c_val);
// void load_program(uint32_t b_val, uint32_t c_val);

#endif