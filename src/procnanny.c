#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "procwatch.h"

#include "memwatch.h"

int main(int argc, char *argv[])
{
        if (2 != argc) {
                errx(EXIT_FAILURE, "Usage: %s [CONFIGURATION FILE]", argv[0]);
        }

        procwatch(argv[1]);

        return EXIT_SUCCESS;
}
