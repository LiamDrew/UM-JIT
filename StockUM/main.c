#include "memory.h"
#include "bitpack.h"

// TODO: I had an issue with a redefinition of stdbool.h when importing files
// Look into this more because it will allow me to significantly improve the
// organization of my program.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void start_um(FILE *fp);

void processor_cycle(u_int32_t *ctr, Memory_T *curr_memory);

int64_t handle_instruction(Memory_T *mem, Instruction word);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./um [executable.um]\n");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(argv[1], "r");

    if (fp == NULL)
    {
        fprintf(stderr, "File could not be opened.\n");
        return EXIT_FAILURE;
    }

    start_um(fp);
    return EXIT_SUCCESS;
}

void start_um(FILE *fp)
{
    // Set program counter to be 0
    u_int32_t *ctr = malloc(sizeof(uint32_t));
    assert(ctr != NULL);
    *ctr = 0;

    // Initialize machine memory
    Memory_T *mem = initialize_memory(fp);

    // Start instruction loop
    processor_cycle(ctr, mem);

    // TODO: Free machine memory when done
    free(ctr);
    free(mem);
}

// takes prgram data and current memory
void processor_cycle(u_int32_t *ctr, Memory_T *curr_memory)
{
    bool keep_running = true;
    while (keep_running)
    {
        /* Fetching next instruction from memory */
        Instruction next_instruction = fetch_instruction(curr_memory, *ctr);

        /* Handling fetched instruction */
        int exit_code = handle_instruction(curr_memory, next_instruction);

        if (exit_code == -2)
        {
            keep_running = false;
            return;
        } 
        
        else if (exit_code >= 0)
        { /* Set program counter to LV*/
            *ctr = (uint32_t)exit_code;
        }
        
        else
        { /* Incrementing Program Counter*/
            *ctr += 1;
        }
    }
}

