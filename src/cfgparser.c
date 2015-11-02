#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <inttypes.h> /*for strtoumax()*/ 
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfgparser.h"
#include "pwwrapper.h"

#define CFG_ONLY
#include "cfgutils.h"
#undef CFG_ONLY

#include "memwatch.h"
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
struct pw_config_info config_parse(int argc, char **argv)
{
        static bool parsing_threshold = true;
        static struct pw_config_info info;

        if (false == parsing_threshold) {
                info.type = PW_PROCESS_NAME;

                while (NULL !=
                       (info.data.process_name = config_parse_pname(argv[1])))
                        return info;

                /*if NULL is returned for the previous loop, we have a EOF*/
                info.type = PW_END_FILE;
                return info;
        } else {
                parsing_threshold = false;
                info.type = PW_WAIT_THRESHOLD;
        }

        if (2 != argc) {
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
        /*
         *dummy variable, since the case is ignored
         *where the first line is not a valid number
         */
        char *endptr = NULL;
        unsigned wait_period = 0;

        if (NULL != fgets(linebuf, sizeof linebuf, nanny_cfg)) {
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
                /*checks overflow error*/
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

        const char *cmd_options[4]     =
        {"sed -n \'2,$p;$q\' ", nanny_cfg_name, " | uniq > ", tmp_fname};
        /*initialize the size to add an extra terminating character*/
        size_t cat_str_size = 1;

        for (size_t i = 0; i < 4; ++i)
                cat_str_size += strlen(cmd_options[i]);

        char *cmd_buffer  = calloc_or_die(1, cat_str_size);

        /*put all the strings in the buffer*/
        for (size_t i = 0; i < 4; ++i)
                strcat(cmd_buffer, cmd_options[i]);

        if (-1 == system(cmd_buffer)) {
                eprintf("system(): Error occurred "
                        "while executing child process");
                exit(EXIT_FAILURE);
        }

        cfree(cmd_buffer);

parse_continue:
        sed_filter_file = fopen_or_die(tmp_fname, "r");
        /*if the file has been opened before, recover its last position*/
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
                        remove(tmp_fname);
                        return NULL;
                } else {
                        perror("fgets()");
                        exit(EXIT_FAILURE);
                }
        }

        fgetpos_or_die(sed_filter_file, &pos);
        fclose_or_die(sed_filter_file);
        /*fgets() leaves the newline character untouched, so kill it*/
        line_buf[strlen(line_buf) - 1] = '\0';
        return line_buf;
}

