#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "castack.h"
#include "procwatch.h"
#include "pwwrapper.h"

#define PW_ONLY
#include "pwutils.h"
#undef PW_ONLY

#include "memwatch.h"

enum { PW_LINEBUF_SIZE = 1024 };
static struct castack *memstack = NULL;

void procwatch(int argc, char **argv)
{
        memstack = castack_init();
        PW_MEMSTACK_ENABLE_AUTO_CLEAN();
        struct pw_config_info info;
        unsigned wait_threshold = 0;

        while (true) {
                info = config_parse(argc, argv);
                switch (info.type) {
                case PW_PROCESS_NAME:
                        printf("%s\n", info.data.process_name);
                        break;
                case PW_WAIT_THRESHOLD:
                        printf("%d\n", info.data.wait_threshold);
                        wait_threshold = info.data.wait_threshold;
                        break;
                case PW_END_FILE:
                        goto procwatch_loop_exit;
                        break;
                }
        }
procwatch_loop_exit:
        ;
}


/*
 *According to the assignment specification,
 *"The program procnanny takes exactly one command-line argument that specifies
 *the full pathname to a configuration file."
 *This function checks the validty of argument vectors and returns a
 *pw_config_info structure that contains info of the config file on subsequent
 *calls on success; otherwise quit the whole program.
 *
 *Note: this function dispatches its 2 kinds of work(threashold or process name)
 *to 2 separate functions.
 */
static struct pw_config_info config_parse(int argc, char **argv)
{
        static bool parsing_threshold = true;
        static struct pw_config_info info;

        if (false == parsing_threshold) {
                info.type = PW_PROCESS_NAME;

                while (NULL !=
                       (info.data.process_name = config_parse_pname(argv[1])))
                        return info;

                //if NULL is returned for the previous loop, we have a EOF
                info.type = PW_END_FILE;
                return info;
        } else {
                parsing_threshold = false;
                info.type = PW_WAIT_THRESHOLD;
        }

        if (argc != 2) {
                eprintf("Usage: %s [FILE]\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        FILE *nanny_cfg = NULL;
        nanny_cfg = fopen_or_die(argv[1], "r");
        info.data.wait_threshold = config_parse_threshold(nanny_cfg);
        fclose_or_die(nanny_cfg);
        return info;
}

/*
 *Parses 1st line of input configuration file and checks its validity:
 *returns an unsigned on success; otherwise quit the program.
 */
static unsigned config_parse_threshold(FILE *const nanny_cfg)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         *the reason for using a statically allocated array rather than
         *something like getline() is that memwatch cannot detect the memory
         *allocated by getline()
         */
        char linebuf[PW_LINEBUF_SIZE] = {};
        // dummy variable, since the case is ignored
        // where the first line is not a valid number
        char *endptr = NULL;
        unsigned wait_period = 0;

        if (NULL != fgets(linebuf, PW_LINEBUF_SIZE, nanny_cfg)) {
                /*From Assignment #1 Description:
                 *"The first line is an integer number indicating how
                 * many seconds procnanny will run for."
                 */
                errno = 0;
                uintmax_t tmp = strtoumax(linebuf, &endptr, 10);
                if (0 == tmp && linebuf == endptr) {
                        eprintf("strtoumax() : "
                                "the first line is not a valid number\n");
                        fclose_or_die(nanny_cfg);
                        exit(EXIT_FAILURE);
                }
                //checks overflow error
                if (UINTMAX_MAX == tmp && ERANGE == errno) {
                        perror("strtoumax()");
                        fclose_or_die(nanny_cfg);
                        exit(EXIT_FAILURE);
                }

                wait_period = tmp;
                if (wait_period != tmp) {
                        eprintf("strtoumax() : "
                                "number of seconds to wait "
                                "exceeds the capacity of an "
                                "unsigned integer\n");
                        fclose_or_die(nanny_cfg);
                        exit(EXIT_FAILURE);
                }
        } else {
                eprintf("The first line does not contain a valid number\n");
                fclose_or_die(nanny_cfg);
                exit(EXIT_FAILURE);
        }
        return wait_period;
}

/*
 *This function receives a file name of the configuraiton file and returns
 *a pointer to each line of the file on each subsequent calls until either
 *EOF is reached or an error occurred.
 *
 *Return Value: NULL upon EOF; char * upon an actual line
 *
 *Note: this function will _not_ close the FILE *nanny_cfg on error
 */
static char *config_parse_pname(const char *const nanny_cfg_name)
{
        static char   line_buf[PW_LINEBUF_SIZE];
        /*
         *this variable indicates the state of this function:
         *whether we have already opened the sed_filter_file or not.
         */
        static bool   in_file                 = false;
        static char   *tmp_fname              = "sed_filter_tmp";
        static fpos_t pos;
        FILE          *sed_filter_file        = NULL;

        if (true == in_file) {
                goto parse_continue;
        }

        enum { SED_FILTER, NANNY_CFG_NAME, UNIQ_FILTER, TMP_FNAME};
        const char *cmd_options[]     =
        {"sed -n \'2,$p;$q\' ", nanny_cfg_name, " | uniq > ", tmp_fname};
        size_t cat_str_size =
                strlen(cmd_options[SED_FILTER])     +
                strlen(cmd_options[NANNY_CFG_NAME]) +
                strlen(cmd_options[UNIQ_FILTER])    +
                strlen(cmd_options[TMP_FNAME])      + 1;
        char *const       cmd_buffer  = castack_push(memstack, 1, cat_str_size);

        for (size_t i = 0; i < sizeof cmd_options / sizeof cmd_options[0]; ++i)
                strcat(cmd_buffer, cmd_options[i]);

        if (-1 == system(cmd_buffer)) {
                eprintf("system(): Error occurred "
                        "while executing child process");
                exit(EXIT_FAILURE);
        }
        // optional, since PW_MEMSTACK_ENABLE_AUTO_CLEAN macro is present
        castack_pop(memstack);

parse_continue:
        sed_filter_file = fopen_or_die(tmp_fname, "r");
        //if the file has been opened before, recover its last position
        if (true == in_file) {
                fsetpos_or_die(sed_filter_file, &pos);
        } else {
                /*
                 *flip the switch if it is the 1st time opened the
                 *sed_filter_file
                 */
                in_file = true;
        }
                
        if (NULL == fgets(line_buf, sizeof line_buf, sed_filter_file)) {
                if (feof(sed_filter_file)) {
                        fclose_or_die(sed_filter_file);
                        return NULL;
                } else {
                        perror("fgets()");
                        exit(EXIT_FAILURE);
                }
        }

        fgetpos_or_die(sed_filter_file, &pos);
        fclose_or_die(sed_filter_file);
        //fgets() leaves the newline character untouched, so kill it
        line_buf[strlen(line_buf) - 1] = '\0';
        return line_buf;
}

static void clean_up(void)
{
        castack_destroy(&memstack);
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
