#include "array.h"

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

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

void array_append(Array_T *arr, void *ptr)
{
    (void)arr;
    (void)ptr;
    assert(false);
}

// void array_insert(Array_T *arr, unsigned idx, void *ptr)
// {
//     (void)arr;
//     (void)idx;
//     (void)ptr;
//     assert(false);
// }

void array_update(Array_T *arr, unsigned idx, uint32_t word)
{
    assert(arr != NULL);
    // printf("Max size is %d, looking at index %d\n", arr->size, idx);
    if (idx == (arr->size + 1)) {
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

void array_remove(Array_T *arr, unsigned idx)
{
    (void)arr;
    (void)idx;
    assert(false);
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

    for (unsigned i = 0; i < length; i++) {
        new_arr->buckets[i] = arr->buckets[i];
    }

    new_arr->size = length;
    new_arr->capacity = (length * 2);

    return new_arr;
}
