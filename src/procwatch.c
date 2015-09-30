#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>


#include "castack.h"
#include "procwatch.h"

#define PW_ONLY
#include "pwutils.h"
#undef PW_ONLY

#include "memwatch.h"


static struct castack *memstack = NULL;

void procwatch(int argc, char **argv)
{
        memstack = castack_init();

        if (atexit(clean_up) != 0) {
                eprintf("atexit(): cannot set exit function\n");
                /*
                 *note this is the only explicit call of castack_destroy():
                 *for all the other cases castack_destroy() is guaranteed
                 *to be called upon exit()
                 */
                castack_destroy(memstack);
                exit(EXIT_FAILURE);
        }

        config_parse(argc, argv);

}

/*
 *According to the assignment specification,
 *"The program procnanny takes exactly one command-line argument that specifies
 *the full pathname to a configuration file."
 *This function simply checks the validty of argument vectors and returns a
 *FILE * on success; otherwise quit the whole program.
 */
static void config_parse(int argc, char **argv)
{
        if (argc != 2) {
                eprintf("Usage: %s [FILE]\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        FILE *nanny_cfg = NULL;

        if ((nanny_cfg = fopen(argv[1], "r")) == NULL) {
                eprintf("%s: missing file operand\n", argv[0]);
                perror("fopen()");
                exit(EXIT_FAILURE);
        }

        config_parse_helper(nanny_cfg);

        if (fclose(nanny_cfg) == EOF) {
                perror("fclose()");
                exit(EXIT_FAILURE);
        }
}

static void config_parse_helper(FILE *const nanny_cfg)
{
        bool firstline = true;

        // dummy variable, since the case is ignored
        // where the first line is not a valid number
        char *endptr = NULL;

        // this character is allocated by getline() automatically
        char *lineptr = NULL;

        // this variable is updated automatically by getline()
        size_t linesize = 0;

        ssize_t num_char_read = 0;
        unsigned wait_period = 0;

        while ((num_char_read = getline(&lineptr, &linesize, nanny_cfg))
               != -1) {
                /*From Assignment #1 Description:
                 *"The first line is an integer number indicating how
                 * many seconds procnanny will run for."
                 */
                if (firstline == true) {
                        uintmax_t tmp = strtoumax(lineptr, &endptr, 10);
                        wait_period = tmp;
                        if (wait_period != tmp) {
                                eprintf("Number of seconds to wait "
                                        "exceeds the capacity of an "
                                        "unsigned integer\n");
                                exit(EXIT_FAILURE);
                        }
                        firstline = false;
                }
                /*
                 *From Assignment #1 Description:
                 *"Each of the other lines of the configuration file are strings
                 * containing the names of programs."
                 */
                // TODO
        }
        // checks whether getline() stops becuase of EOF
        free(lineptr);
}
static void clean_up(void)
{
        castack_destroy(memstack);
}
        /*
        if (castack_empty(castack_ptr))
                puts("It is empty!");
        int *arr_ptr = castack_push(castack_ptr, 4, sizeof(int));
        for (int i = 0; i < 4; ++i)
                printf("%d\t", arr_ptr[i]);
        putchar('\n');
        if (!castack_empty(castack_ptr))
                printf("Has %zu elements\n", castack_report(castack_ptr));
        castack_destroy(castack_ptr);
        */
