#include "virt.h"
#include <sys/mman.h>

Mem_T *mem = NULL;
uint8_t *usable = NULL;
Stack_T *rec = NULL;
uint32_t start_unused;

static inline Stack_T stack_init(uint32_t size);

static inline void stack_free(Stack_T s);

uint8_t *init_memory_system(uint32_t kernel_size)
{
    /* Safely initialize the memory state */
    assert(mem == NULL);
    mem = (Mem_T *)malloc(sizeof(Mem_T));
    assert(mem != NULL);

    /* Allocate 4 GB of contiguous virtual memory */
    void *virt = mmap(NULL, GB4, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    usable = ((uint8_t *)virt + BOOK_SIZE);

    mem->mem = virt;
    mem->usable_mem = (void *)((uint8_t *)virt + BOOK_SIZE);
    rec = recycler_init();
    mem->recycler = rec;

    /* The kernel virtual size is 8 bytes smaller than its physical size */
    mem->kernel_virtual_size = kernel_size - BOOK_SIZE;
    mem->begin_unused = kernel_size;
    start_unused = kernel_size;

    return usable;
}

void terminate_memory_system(void)
{
    /* Free the memory object statically defined within this file */
    munmap(mem->mem, GB4);
    free_recycler(rec);
}

/* Kernel (Re)allocate (kern_realloc):
 * Overwrite the zero segment and initialize all memory to zero */
uint32_t kern_realloc(uint32_t size)
{
    /* Ensure the kernel has enough space to fill the allocation */
    assert(mem->kernel_virtual_size >= size);

    /* Update the first 8 bytes of virtual memory with kernel bookkeeping */
    uint32_t *mem_start = (uint32_t *)mem->mem;
    *mem_start = mem->kernel_virtual_size;
    mem_start++;
    *mem_start = size;

    /* The base kernel virtual address is always 0. */
    return 0;
}

/* Kernel Memory Copy (kern_memcpy):
 * Copies data from "userspace" to "kernel space" */
void kern_memcpy(uint32_t src_addr, uint32_t copy_size)
{
    /* This function will overwrite the kernel memory (segment 0). The user
     * does not control the destination this memory is copied to; the kernel
     * does. */

    /* Get real source and destination addresses to use with memcpy */
    uint8_t *umem = usable;
    void *real_src = convert_address(umem, src_addr);
    void *real_dest = convert_address(umem, 0);
    memcpy(real_dest, real_src, copy_size);
    return;
}

inline Stack_T stack_init(uint32_t size)
{
    assert(size > 0);

    Stack_T s;
    s.stack = malloc(size * sizeof(uint32_t));
    s.capacity = size;
    s.size = 0;

    return s;
}

inline void stack_free(Stack_T s)
{
    free(s.stack);
}

Stack_T *recycler_init(void)
{
    Stack_T *recycler = malloc(sizeof(Stack_T) * REC_BUCKETS);
    assert(recycler != NULL);

    for (uint32_t i = 0; i < REC_BUCKETS; i++)
    {
        Stack_T s = stack_init(INIT_STACK_SIZE);
        recycler[i] = s;
    }

    return recycler;
}

void free_recycler(Stack_T *rec)
{
    for (uint32_t i = 0; i < REC_BUCKETS; i++)
    {
        stack_free(rec[i]);
    }

    free(rec);
}
