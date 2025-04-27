/*
 * For ARM64 on MacOS, the following registers are non-volatile
 * X19-X29 */

#include <stdio.h>
#include "utility.h"
#include "stdint.h"

typedef void *(*Function)(void);

void help();

void help() {
    int x = 0x12345678;
    (void)x;
}

int main(int argc, char *argv[]) {
    // uint32_t x = 19;
    // uint32_t y = ~x + 1; // to convert to negative, flip the bits and add 1
    // printf("Y value is %d\n", y);

    printf("Hi there\n");
    unsigned x = test();
    printf("X is %u\n", x);

    return 0;
}
