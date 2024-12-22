/**
 * @file bitpack.c
 * @authors Eyal Sharon and Liam Drew
 * @brief The implementation of the bitpack module. Supports the storage
 *        of information in a more compact format, as well as supporting the
 *        access to the stored information.
 * @date March 2023
 */

#include "bitpack.h"
#include <assert.h>

// Except_T Bitpack_Overflow = {"Overflow packing bits"};

/**********Bitpack_fitsu ********
Purpose: Determine if an unsigned int can be represented in a given number of
    bits.

Input:
    n: the unsigned 64-bit int attempting to be represented
    width: the available number of bits to represent the uint

Effects: Returns true if n "fits" in width bits, and false if it doesn't fit.

********************************/
bool Bitpack_fitsu(uint64_t n, unsigned width)
{
    assert(width <= 64);

    /* If field width is 0, we return true if n is 0, and false otherwise, per
     * page 11 of the spec.
     */

    if (width == 0)
    {
        return (n == 0);
    }

    /* Splitting up the shifting to account for the 64 bit shift edge case */
    uint64_t size = (uint64_t)1 << (width - 1);
    size = size << 1;
    size -= 1;

    return (size >= n);
}

/**********Bitpack_fitss ********
Purpose: Determine if a int can be represented in a given number of
    bits.

Input:
    n: the signed 64-bit int attempting to be represented
    width: the available number of bits to represent the int

Effects: Returns true if n "fits" in width bits, and false if it doesn't fit.

********************************/
bool Bitpack_fitss(int64_t n, unsigned width)
{
    assert(width <= 64);

    /* Same logic as the previous function */
    if (width == 0)
    {
        return (n == 0);
    }

    /* 64 bit shift is not possible here */
    int64_t magnitude = (int64_t)1 << (width - 1);

    /* Determining the bounds of n based on 2's compliment */
    int64_t upper_bound = magnitude - 1;
    int64_t lower_bound = -1 * magnitude;

    return (n >= lower_bound && n <= upper_bound);
}

/**********Bitpack_getu ********
Purpose: Get the bits stored at a certain position in a uint64 and return
    the interpreted results of those bits as an unsigned int.

Input:
    word: the unsigned 64-bit int being parsed
    width: the width of the bit field the client is looking for
    lsb: the least significant bit in the word that the client cares about

Effects: Returns the unsigned int representation of the accessed bits.

********************************/
uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb)
{
    /* Asserting necessary conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    /* Fields of width 0 contain value 0 */
    if (width == 0)
    {
        return 0;
    }

    word = word >> lsb;
    uint64_t mask = 0;

    /* Returns the part of the word we care about */
    mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;

    return (word & mask);
}

/**********Bitpack_gets ********
Purpose: Get the bits stored at a certain position in a uint64 and return
    the interpreted results of those bits as an signed int.

Input:
    word: the unsigned 64-bit int being parsed
    width: the width of the bit field the client is looking for
    lsb: the least significant bit in the word that the client cares about

Effects: Returns the signed int representation of the accessed bits.

********************************/
int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb)
{
    /* Asserting conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    /* Fields of 0 contain value 0 */
    if (width == 0)
    {
        return 0;
    }

    word = word >> lsb;
    uint64_t mask = 0;
    mask -= 1;

    /* Splitting up mask shifting */
    mask = mask << (width - 1);
    mask = mask << 1;
    mask = ~mask;

    /* Returns the bits of interest */
    int64_t new_int = (word & mask);

    /* Shifts and shifts back in order to propagate the signed bit */
    new_int = new_int << (64 - width);
    new_int = new_int >> (64 - width);

    return new_int;
}

/**********Bitpack_newu ********
Purpose: Updates the bits in a uint64 in a particular range to store inputted
    uint64 data.

Input:
    word: the unsigned 64-bit int being updated
    width: the width of the bit field being modified
    lsb: the least significant bit in the word that is being modified
    value: the modified uint64 value being inserted in the accessed range

Effects: Returns the updated unsigned 64-bit int containing the inputted
    information.

********************************/
uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb, uint64_t value)
{
    /* Asserting Conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    /* Check to make sure value can fit in number of bits */
    if (!Bitpack_fitsu(value, width))
    {
        // RAISE(Bitpack_Overflow);
        // Moving everything to native
        assert(false);
        return word;
    }

    /* If value and width are 0 and lsb is 64, return word */
    if (width == 0)
    {
        return word;
    }

    uint64_t mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;

    /* There is no way for lsb to be 64 here, since either an earlier assert
     * would have failed or the function would have returned 0 */
    mask = mask << lsb;
    mask = ~mask;

    uint64_t new_word = (word & mask);

    value = value << lsb;
    uint64_t return_word = (new_word | value);

    return return_word;
}

/**********Bitpack_news ********
Purpose: Updates the bits in a uint64 in a particular range to store inputted
    signed int data.

Input:
    word: the unsigned 64-bit int being updated
    width: the width of the bit field being modified
    lsb: the least significant bit in the word that is being modified
    value: the modified int64 value being inserted in the accessed range

Effects: Returns the updated unsigned 64-bit int containing the inputted
    information.

********************************/
uint64_t Bitpack_news(uint64_t word, unsigned width, unsigned lsb, int64_t value)
{
    /* Asserting Conditions */
    assert(width <= 64);
    assert((width + lsb) <= 64);

    if (!Bitpack_fitss(value, width))
    {
        // RAISE(Bitpack_Overflow);
        // Moving to native
        assert(false);
        return 0;
    }

    if (width == 0)
    {
        return word;
    }

    uint64_t mask = (uint64_t)1 << (width - 1);
    mask = mask << 1;
    mask -= 1;
    mask = mask << lsb;
    mask = ~mask;

    uint64_t new_word = (word & mask);

    value = value << lsb;
    uint64_t cast = (uint64_t)value;

    /* Shifting and reshifting to propagate the signed bit */
    cast = cast << (64 - width - lsb);
    cast = cast >> (64 - width - lsb);

    uint64_t return_word = (new_word | cast);
    return return_word;
}
