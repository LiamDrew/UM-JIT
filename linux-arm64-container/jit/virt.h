#ifndef VIRT_H
#define VIRT_H

#define BOOK_SIZE 8
#define MIN_SEG_SIZE 32

/* The user can allocate at most 2^24 - 8 bytes. This means there are 2^19
 * different segment lengths a user can allocate, and we are going to use a
 * unique bucket to recycle each one. */
#define MAX_ALLOC (((uint32_t)1 << 24) - BOOK_SIZE)
#define KERN_SIZE MAX_ALLOC
#define REC_BUCKETS ((uint32_t)1 << 19) /* 2^19 recyclable segment lengths */

#define GB4 ((uint64_t)1 << 32) /* 4 GB = 2^32 */
#define BOOK_SIZE 8
#define BLOCK_SIZE 32

#define INIT_STACK_SIZE 2
#define SEG_NOT_FOUND 1

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef struct
{
    uint32_t *stack;
    uint32_t size;
    uint32_t capacity;
} __attribute__((packed)) Stack_T;

typedef struct
{
    void *mem;        /* Pointer to the full 4GB of memory */
    void *usable_mem; /* Pointer to the beginning of usable memory */
    void *recycler;   /* Array of stacks for recycling segments */
    uint32_t kernel_virtual_size;
    uint32_t begin_unused;
} Mem_T;

extern uint8_t *usable;
extern Stack_T *rec;
extern Mem_T *mem;
extern uint32_t start_unused;

/* Memory utility functions */

/* Using a macro is disgusting but the linker gave me no choice */
#define convert_address(umem, addr) ((void *)((uint8_t *)umem + addr))

inline uint32_t get_idx_from_alloc_size(uint32_t size)
{
    /* Will allocate 1 block when it gets an exact fit */
    uint32_t num_blocks = ((size + BOOK_SIZE - 1) / 32);
    return num_blocks;
}

/* Stack interface */
inline uint32_t stack_top(Stack_T s)
{
    assert(s.size > 0);
    return s.stack[s.size - 1];
}

inline Stack_T stack_push(Stack_T s, uint32_t elem)
{
    if (s.size == s.capacity)
    {
        /* Expand the stack */
        s.capacity *= 2;
        uint32_t *temp = malloc(s.capacity * sizeof(uint32_t));
        for (uint32_t i = 0; i < s.size; i++)
        {
            temp[i] = s.stack[i];
        }

        free(s.stack);
        s.stack = temp;
    }

    s.stack[s.size++] = elem;
    return s;
}

inline Stack_T stack_pop(Stack_T s)
{
    /* Omitted to improve performance:
     * assert(s.size > 0); */

    s.size--;
    return s;
}

inline bool stack_is_empty(Stack_T s)
{
    return s.size == 0;
}

/* Recycler functions*/
Stack_T *recycler_init(void);

void free_recycler(Stack_T *rec);

inline uint32_t find_freed_segment(uint32_t size, Stack_T *rec)
{
    uint32_t index = get_idx_from_alloc_size(size);

    /* Omitted to improve performance:
     * assert(index < REC_BUCKETS); */

    Stack_T s = rec[index];

    /* if no segment of size 'size', check next bucket */
    if (stack_is_empty(s))
        return SEG_NOT_FOUND;

    uint32_t freed_segment = stack_top(s);
    rec[index] = stack_pop(s);
    return freed_segment;
}

inline void free_segment(uint8_t *umem, uint32_t seg_addr, Stack_T *rec)
{
    uint32_t sys_addr = seg_addr - BOOK_SIZE;

    uint32_t *virt = convert_address(umem, sys_addr);
    uint32_t cap = *virt;

    uint32_t index = ((cap + 8) / 32) - 1;

    /* NOTE: intentionally storing the user-facing v^2 address in the stack
     * for easy reuse in the future */
    rec[index] = stack_push(rec[index], seg_addr);
}

/* Memory system interface */

uint8_t *init_memory_system(uint32_t kernel_size);

void terminate_memory_system(void);

uint32_t kern_realloc(uint32_t size);

void kern_memcpy(uint32_t src_addr, uint32_t copy_size);

/* Virtual Segment Calloc (vs_calloc):
 * Carve out a segment of virtual memory and serve it to the program as
 * zeroed-out v^2 memory */
inline uint32_t vs_calloc(uint8_t *umem, uint32_t size)
{
    /* Users may only ask vs_malloc for (2^24 - 8) bytes of contiguous space
     * Omitted for performance reasons. The user must use our module correctly:
     * assert(size < MAX_ALLOC); */

    /* Look for segments to be recycled. If there are freed segments that are
     * ready to be recycled, recycled them */
    uint32_t freed_seg = find_freed_segment(size, rec);

    /* check that a free segment is available */
    if (freed_seg != SEG_NOT_FOUND)
    {
        uint32_t *freed_seg_addr = convert_address(umem, freed_seg);

        freed_seg_addr[-1] = size;

        memset(freed_seg_addr, 0, size);

        return freed_seg;
    }

    /* If no segments can be recycled, carve a fresh one from the heap */
    uint32_t user_start = start_unused + BOOK_SIZE;

    /* Find the number of 32 byte blocks need to fill the allocation */
    uint32_t num_blocks = get_idx_from_alloc_size(size) + 1;
    uint32_t user_cap = (num_blocks * BLOCK_SIZE) - BOOK_SIZE;

    /* Check that we still have enough 'carvable' memory in the heap.
     * Add BOOK_SIZE to user start to account for kernel bookkeeping
     * Omitting this for performance reasons:
     * assert(GB4 - (user_start + BOOK_SIZE) >= user_cap); */

    /* Update the beginning of the unused heap */
    start_unused = user_start + user_cap;

    uint32_t *user_addr = convert_address(umem, user_start);

    user_addr[-2] = user_cap;
    user_addr[-1] = size;

    return user_start;
}

/* Virtual Segment Free (vs_free):
 * Free a virtual segment for future use. */
inline void vs_free(uint32_t addr)
{
    free_segment(usable, addr, rec);
}

/* Set At (set_at):
 * Store a uint32_t at a virtual address */
inline void set_at(uint8_t *umem, uint32_t addr, uint32_t value)
{
    uint32_t *dest = convert_address(umem, addr);
    *dest = value;
}

/* Get At (get_at):
 * Get the uint32_t stored at a virtual address */
inline uint32_t get_at(uint8_t *umem, uint32_t addr)
{
    uint32_t *src = convert_address(umem, addr);
    return *src;
}

#endif
