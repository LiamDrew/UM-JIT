#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "um_utils.h"
#include "arpa/inet.h"

unsigned char read_char(void)
{
    printf("Global state seq size: %d\n", gs.seq_size);
    printf("Function was called\n");
    int x = getc(stdin);
    assert(x != EOF);
    return (unsigned char)x;
}

uint32_t map_segment(uint32_t size)
{
    (void)size;
    printf("Raw value is %u (0x%x)\n", size, size);

    // Size will be stored in register c
    // it will be passed in as an argument to the function

    // mmap something

    // return the address of the mmaped memory
    // return;

    // the injected assembly will store the address in register b
    // Store memory address in register b
    // assert(false);
    return 0;
}

// void unmap segment(void *segmentId)
void unmap_segment(uint32_t segmentID)
{
    (void)segmentID;
    assert(false);
}