/*
 * For ARM64 on MacOS, the following registers are non-volatile
 * X19-X29 */

#include <stdio.h>

void help();

int main(int argc, char *argv[]) {

    // int x = 0x12345678;
    // printf("%d\n", x);
    help();
    return 0;
}

void help() {
    int x = 0x12345678;
    (void)x;
}