#ifndef ARRAY_H__
#define ARRAY_H__

// To start, we'll do the array with void pointers. We may switch to a more
// performant option
#include <stdlib.h>

typedef struct {
    unsigned size;
    unsigned capacity;
    uint32_t *buckets;
    // void **buckets; // void pointer, but could just be a uint32. review this
} Array_T;

Array_T *new_array(unsigned capacity);

void free_array(Array_T **arr);

void array_append(Array_T *arr, void *ptr);

// void array_insert(Array_T *arr, unsigned idx, void *ptr);
// void array_insert(Array_T *arr, unsignd idx, uint32_t ptr);

void array_update(Array_T *arr, unsigned idx, uint32_t word);

uint32_t array_at(Array_T *arr, unsigned idx);

void array_remove(Array_T *arr, unsigned idx);

Array_T *array_copy(Array_T *arr, unsigned length);

unsigned array_capacity(Array_T *arr);

unsigned array_size(Array_T *arr);


#endif