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

    const char *filename = "bigadd.um";

    FILE *fp = fopen(filename, "wb");
    assert(fp != NULL);

    // use fwrite()

    Instruction words[15] = {0};

    size_t bw = 14;

    // r3 = 1
    words[0] = load_val(13, 1, 3);

    // load 67 into r2
    words[1] = load_val(13, 67, 2);

    // nand r6 with r6 and put result in r1
    words[2] = three_reg(6, 1, 6, 6);

    // print out r1 result
    words[3] = three_reg(10, 0, 0, 1);

    // add r1 to r2, store in r2
    words[4] = three_reg(3, 2, 2, 1);

    // print out r2 result
    words[5] = three_reg(10, 0, 0, 2);

    // r7 = 2
    words[6] = load_val(13, 2, 7);

    // r0 = r2 nand r2
    words[7] = three_reg(6, 0, 2, 2);

    // printout r0
    words[8] = three_reg(10, 0, 0, 0);

    // r7 = r7 + r0
    words[9] = three_reg(3, 7, 7, 0);

    // printout r7
    // words[10] = three_reg(10, 0, 0, 7);

    words[10] = three_reg(0, 0, 0, 0);

    // The isse is the ORDER of operations for addition. Say what?
    //r7 = r3 + r7
    // r7 = r7 + r3
    words[11] = three_reg(3, 7, 3, 7);

    // printout r7
    words[12] = three_reg(10, 0, 0, 7);

    // load program to print out value
    words[13] = three_reg(7, 0, 0, 0);
    // words[13] = three_reg(12, 0, 4, 7);



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

// // load the value 49 into reg 7
// words[0] = load_val(13, 49, 7);

// // the 0 reg will remain 0
// // the 1 reg will contain 1
// words[1] = load_val(13, 1, 1);

// // reg 5 contains the index of the first instruction getting loaded
// words[2] = load_val(13, 11, 5);

// // map segment of size 48, result gets put in r2
// words[3] = three_reg(8, 0, 2, 7);

// //output r7
// words[4] = three_reg(10, 0, 0, 7);

// // seg load into r6
// words[5] = three_reg(1, 6, 0, 5);

// // seg store r6 into segment 1 @index 0
// words[6] = three_reg(2, 1, 0, 6);

// // add one to r5, the instruction loaded index
// words[7] = three_reg(3, 5, 5, 1);

// // seg load into r6 again
// words[8] = three_reg(1, 6, 0, 5);

// // seg store r6 into seg 1 @ index 1
// words[9] = three_reg(2, 1, 1, 6);

// // load program
// words[10] = three_reg(12, 0, 1, 0);
// // this code is only for getting mapped and not getting executed

// // print reg 7
// words[11] = three_reg(10, 0, 0, 7);

// //halt
// words[12] = three_reg(7, 0, 0, 0);
