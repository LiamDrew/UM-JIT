#ifndef BITPACK_H__
#define BITPACK_H__

#include <stdint.h>
#include <stdbool.h>

uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb);
int64_t Bitpack_gets(uint64_t word, unsigned width, unsigned lsb);

uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb, uint64_t value);
uint64_t Bitpack_news(uint64_t word, unsigned width, unsigned lsb, int64_t value);

bool Bitpack_fitsu(uint64_t n, unsigned width);
bool Bitpack_fitss(int64_t n, unsigned width);

void printBinary(uint64_t num, char *name);

#endif
