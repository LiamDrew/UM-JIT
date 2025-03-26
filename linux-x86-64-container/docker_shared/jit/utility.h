#ifndef UTILITY_H
#define UTILITY_H

// #define CHUNK 10
#define CHUNK 13


#define OP_MAP 0
#define OP_UNMAP 1
#define OP_OUT 2
#define OP_IN 3
#define OP_DUPLICATE 4
#define OP_RECOMPILE 5
#define OP_HALT 6

#define OP_LOAD 8
#define OP_STORE 9

#ifndef __ASSEMBLER__
    // void run(uint8_t *zero, uint32_t **seq_addr);
    void run(uint8_t *zero, uint8_t *umem);
#endif

#endif
