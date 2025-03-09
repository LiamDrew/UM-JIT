#ifndef UTILITY_H
#define UTILITY_H

// #include <stdio.h>

// #define CHUNK 29
#define CHUNK 32

// Huge thank you to Tom for showing me the way with this
#define OP_MAP 0
#define OP_UNMAP 1
#define OP_OUT 2
#define OP_IN 3
#define OP_DUPLICATE 4
#define OP_RECOMPILE 5

#ifndef __ASSEMBLER__
    void run(uint8_t *zero);
    // void run(uint32_t num);
#endif

#endif
