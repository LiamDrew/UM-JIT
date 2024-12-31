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
    printf("Raw value is %u (0x%x)\n", size, size);

    // Size will be stored in register c
    // it will be passed in as an argument to the function

    // mmap something

    // return the address of the mmaped memory
    // return;

    // the injected assembly will store the address in register b
    // Store memory address in register b
    // assert(false);

    return 8008135;
}

// void unmap segment(void *segmentId)
void unmap_segment(uint32_t segmentId)
{
    printf("Seg id is: %u\n", segmentId);
    // TODO: do the unmapping
}

// segmented load
uint32_t segmented_load(Instruction word)
{
    uint32_t a_val;
    uint32_t b_val;
    uint32_t c_val;
    (void)a_val;
    (void)b_val;
    (void)c_val;
    (void)word;

    // In this case, we strictly get the value, since it would make no sense to load a bunch of assembly instruction into a register

    // TODO: unpack the word into the stuff


    assert(false);
}

// segmented store (will have to compile r[C] to machine code inline)
/*
 * Since this function may have to compile assembly inline, it may have to be
 * called by 8 byte function pointer instead of 4 byte offset (if it wants to live in main
 * and have access to all the functions that can access the other functions)
 * 
 * Alternatively, everything could get moved into this utils file except the main function.
 */
void segmented_store(Instruction word)
{
    (void)word;

    // TODO: unpack the word into the stuff

    // In this case, we have to both compile to machine code and store the native UM word
    return;
}

// load program
/* Load program needs to do something rather important:
 * update the memory address of the new segment being executed
 */
void *load_program(uint32_t b_val, uint32_t c_val)
{
    (void)b_val;
    (void)c_val;
    // set program counter to be the right thing

    // TODO: print out the right stuff

    return NULL;
}