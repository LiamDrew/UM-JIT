#include "instruction_handler.h"
#include "bitpack.h"
#include "io.h"

int64_t handle_instruction(Memory_T *mem, Instruction word)
{
    /* decode the opcode, a, b, c*/
    unsigned int a = 0, b = 0, c = 0;
    uint32_t value_to_load = 0;
    unsigned int opcode = Bitpack_getu(word, 4, 28);
    if (opcode == 13)
    {
        /* Load value into $r[A] */
        a = Bitpack_getu(word, 3, 25);
        value_to_load = Bitpack_getu(word, 25, 0);
    }

    else
    {
        c = Bitpack_getu(word, 3, 0);
        b = Bitpack_getu(word, 3, 3);
        a = Bitpack_getu(word, 3, 6);
    }

    /* power stores 2^32 for use with arithmetic operations */
    uint64_t power = (uint64_t)1 << 32;

    /* temp is used in some arithmetic operations to prevent integer
     * overflow when adding and multiplying uint32_ts */

    uint64_t temp;
    /* In this switch table, we handle every single UM instruction */
    bool mem_exhausted = false;

    switch (opcode)
    {
    case 0:
        /* Conditional Move */
        // This could be rewritten at some point, but now's not the time
        if (get_register(mem, c) == 0) {
            break;
        } else {
            set_register(mem, a, get_register(mem, b));
        }

        break;
    case 1:
        /* Segmented Load (Memory Module) */
        set_register(mem, a, segmented_load(mem, get_register(mem, b), get_register(mem, c)));
        break;
    case 2:
        /* Segmented Store (Memory Module) */
        segmented_store(mem, get_register(mem, a),
                        get_register(mem, b), get_register(mem, c));
        break;
    case 3:
        /* Addition */
        temp = get_register(mem, b);
        temp += get_register(mem, c);
        set_register(mem, a, (temp % power));
        break;
    case 4:
        /* Multiplication */
        temp = get_register(mem, b);
        temp *= get_register(mem, c);
        set_register(mem, a, (temp % power));
        break;
    case 5:
        /* Division */
        temp = get_register(mem, b);
        temp = (temp / get_register(mem, c));
        set_register(mem, a, temp);
        break;
    case 6:
        /* Bitwise NAND */
        set_register(mem, a,
                     ~(get_register(mem, b) & get_register(mem, c)));
        break;
    case 7:
        /* Halt */
        handle_halt(mem);
        /* This exit code of -2 tells the operations module to
         * stop the processor cycle */
        return -2;
        break;
    case 8:
        /* Map Segment (Memory Module) */
        // printf("Mapping segment\n");

        set_register(mem, b, map_segment(mem, get_register(mem, c), &mem_exhausted));
        if (mem_exhausted)
        {
            printf("Mem has been exhausted\n");
            handle_halt(mem);
            return -2;
        }
        break;
    case 9:

        // printf("UNMAPping segment\n");
        /* Unmap Segment (Memory Module) */
        unmap_segment(mem, get_register(mem, c));
        break;
    case 10:
        /* Output (IO) */
        output_register(get_register(mem, c));
        break;
    case 11:
        /* Input (IO) */
        set_register(mem, c, read_in_to_register());
        break;
    case 12:
        // printf("Loading the program\n");
        /* Load Program (Memory Module) */
        load_program(mem, get_register(mem, b));
        /* This exit code returns the new value of the program
         * counter */
        return (int64_t)get_register(mem, c);
    case 13:
        /* Load Value */
        set_register(mem, a, value_to_load);
        break;
    }

    /* This exit code of -1 means everything is normal */
    return -1;
}