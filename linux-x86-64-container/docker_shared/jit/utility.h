#ifndef UTILITY_H
#define UTILITY_H

#define CHUNK 10

#define OP_MAP 1
#define OP_UNMAP 2
#define OP_OUT 3
#define OP_IN 4
#define OP_DUPLICATE 5
#define OP_HALT 6

#ifndef __ASSEMBLER__
    void run(uint8_t *zero, uint8_t *umem);
#endif

#endif
