#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"

void compileFile(const char *fileName)
{
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: eplc filename");
        return 0;
    }
    compileFile(argv[1]);

    return 0;
}
