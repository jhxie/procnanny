#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
        if (3 != argc) {
                exit(1);
        }

        /*token to be parsed*/
        char *str = strdup(argv[1]);
        /*delimeters*/
        const char *dem = argv[2];
        char *token;
        char *saveptr;

        printf("The originl string is %s\n", str);

        token = strtok_r(str, dem, &saveptr);
        printf("%s\n", token);
        while (NULL != (token = strtok_r(NULL, dem, &saveptr))) {
                printf("%s\n", token);
        }
        return 0;
}
