#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>

typedef uint32_t Instruction;

// Constants for `three_reg`
#define OPCODE_SHIFT 28
#define A_SHIFT 6
#define B_SHIFT 3
#define C_SHIFT 0

// Constants for `load_val`
#define VAL_BITS 25
#define A_VAL_SHIFT 25

Instruction three_reg(uint32_t opcode, uint32_t a, uint32_t b, uint32_t c);
Instruction load_val(uint32_t opcode, uint32_t val, uint32_t a);
void decode_instruction(uint32_t word);

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const char *filename = "map.um";

    FILE *fp = fopen(filename, "wb");
    assert(fp != NULL);

    // use fwrite()

    Instruction words[10] = {0};

    size_t bw = 3;

    // load 3 into reg 1
    words[0] = load_val(13, 3, 1);

    words[1] = three_reg(8, 0, 2, 1);

    words[2] = three_reg(7, 0, 0, 0);


    // NOTE: must write bytes to disk in big endian order
    for (size_t i = 0; i < bw; i++)
    {
        words[i] = htonl(words[i]);
    }

    size_t written = fwrite(words, sizeof(Instruction), bw, fp);
    assert(written == bw); // Ensure all 8 instructions were written

    return 0;
}


Instruction three_reg(uint32_t opcode, uint32_t a, uint32_t b, uint32_t c)
{
    assert(a < 8);
    assert(b < 8);
    assert(c < 8);
    assert(opcode < 13);

    Instruction word = 0;
    // first 4 bits are the opcode
    // word = word | (opcode << 28);
    word = opcode;
    word = word << 28;

    // bits 6 thru 9 are register a
    word = word | (a << 6);

    // bits 3 thru 6 are register b
    word = word | (b << 3);

    // last 3 bits are register c
    word = word | c;

    return word;
}

Instruction load_val(uint32_t opcode, uint32_t val, uint32_t a)
{
    assert(a < 8);
    assert(opcode == 13);

    Instruction word = 0;
    // first 4 bits are the opcode
    word = word | (opcode << 28);

    // next 3 bits are register a
    word = word | (a << 25);

    // remaining 25 bits are the value
    word = word | (val);

    return word;
}
// Instruction load_val(uint32_t opcode, uint32_t val, uint32_t a)
// {
//     assert(a < 8);            // Register `a` is 3 bits
//     assert(opcode == 13);     // Opcode should be 13
//     assert(val < (1U << 25)); // Value must fit in 25 bits

//     Instruction word = 0;
//     word |= (opcode & 0xF) << 28; // Opcode in bits 28–31
//     word |= (a & 0x7) << 25;      // Register `a` in bits 25–27
//     word |= (val & 0x1FFFFFF);    // Value in bits 0–24

//     return word;
// }

void decode_instruction(uint32_t word)
{
    // Extract the opcode (bits 28-31)
    uint32_t opcode = (word >> 28) & 0xF; // Mask with 0xF to extract 4 bits

    printf("Opcode: %u\n", opcode);

    if (opcode == 13)
    {
        // This is a load_val instruction
        uint32_t a = (word >> 25) & 0x7; // Extract register A (bits 25-27)
        uint32_t val = word & 0x1FFFFFF; // Extract value (bits 0-24)

        printf("Register A: %u\n", a);
        printf("Value: %u\n", val);
    }
    else
    {
        // This is a three_reg instruction
        uint32_t a = (word >> 6) & 0x7; // Extract register A (bits 6-8)
        uint32_t b = (word >> 3) & 0x7; // Extract register B (bits 3-5)
        uint32_t c = word & 0x7;        // Extract register C (bits 0-2)

        printf("Register A: %u\n", a);
        printf("Register B: %u\n", b);
        printf("Register C: %u\n", c);
    }
}
