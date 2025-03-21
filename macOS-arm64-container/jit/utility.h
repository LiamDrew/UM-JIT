#ifndef UTILITY_H
#define UTILITY_H

// #define CHUNK 24 // will have to be expanded
#define CHUNK 20 // will have to be expanded
#define BR 19    // base general purpose register
#define OP_REG 14

// Huge thank you to Tom for showing me the way with this
#define OP_MAP 0
#define OP_UNMAP 1
#define OP_OUT 2
#define OP_IN 3
#define OP_DUPLICATE 4
#define OP_RECOMPILE 5

#ifndef __ASSEMBLER__
void run(uint8_t *zero, uint32_t **seg_table);
#endif

#endif
