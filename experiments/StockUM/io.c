#include "io.h"

#include <stdio.h>
#include <assert.h>

void output_register(uint32_t register_contents)
{
    /* Assert it's a char*/
    assert(register_contents <= 255);
    /* Print the char*/
    unsigned char c = (unsigned char)register_contents;
    printf("%c", c);
}

uint32_t read_in_to_register()
{
    int c;
    uint32_t contents = 0;
    c = getc(stdin);
    /* Check it's not an EOF character*/
    if (c == EOF)
    {
        contents -= 1;
        // fprintf(stderr, "contents is %u \n", contents);
        return contents;
    }
    else
    {
        assert(c <= 255);
        contents = (uint32_t)c;
        return contents;
    }
}