#include <stdio.h>
#include <stdlib.h>

#include "procwatch.h"
#include "pwwrapper.h"
#include "memwatch.h"

int main(int argc, char *argv[])
{
        if (2 != argc) {
                eprintf("Usage: %s [CONFIGURATION FILE]\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        procwatch(argv[1]);

        return EXIT_SUCCESS;
}
