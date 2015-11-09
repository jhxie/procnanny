/*
 *Note this module contains a file scope variable pw_cfg_vector with EXTERNAL
 *LINKAGE, which is used to store the configuration file information;
 *normally the information within the vector should ONLY BE INSPECTED rather
 *than MODIFIED.
 */
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
#include "pwlog.h"

#define CFG_ONLY
#include "cfgutils.h"
#undef CFG_ONLY

#include "memwatch.h"

/*
 *According to the specification, "You can safely assume that the input file
 *will contain no more than 128 program names"; I also make the assumption that
 *there is no redundant entries in the configuraiton file
 *(detailed in #3 ASSUMPTION in README.md).
 */
struct pw_cfg_info pw_cfg_vector[PW_CFG_MAX_NUM_PROGRAM_NAME];

static size_t pw_cfg_idx                = 0;
static const char *const CFG_WHITESPACE = "\t ";
static const int WAIT_THRESHOLD_BASE    = 10;

void config_parse(const char *const cfgname)
{
        if (PW_CFG_MAX_NUM_PROGRAM_NAME == pw_cfg_idx) {
                eprintf("The 128 program name assumption in the specification"
                        " is violated, terminate the program.");
                exit(EXIT_FAILURE);
        }

        /*
         *Note here the saveptr is used for strtok_r to recover the context
         *left off by the first call; which occurred in config_parse_pname().
         */
        char *saveptr = NULL;
        char linebuf[PW_LINEBUF_SIZE] = {};
        /*for extra safety*/
        memset(linebuf, 0, PW_LINEBUF_SIZE);
        FILE *nanny_cfg = fopen_or_die(cfgname, "r");

        while (NULL != fgets(linebuf, PW_LINEBUF_SIZE, nanny_cfg)) {
                /*fgets() leaves the newline character untouched, so strip it*/
                linebuf[strlen(linebuf) - 1] = '\0';
                /*
                 *it is safe to use strcpy here since the length of a token
                 *within a string MUST be less than or equal to the string
                 *itself
                 */
                strcpy(pw_cfg_vector[pw_cfg_idx].process_name,
                        config_parse_pname(linebuf, &saveptr));

                pw_cfg_vector[pw_cfg_idx].wait_threshold =
                        config_parse_threshold(linebuf, saveptr);
                saveptr = NULL;
                pw_cfg_idx++;
        }

        /*Check whether the NULL returned by fgets() is caused by EOF*/
        if (feof(nanny_cfg)) {
                fclose_or_die(nanny_cfg);
        } else {
                perror("fgets()");
                exit(EXIT_FAILURE);
        }
}

static unsigned config_parse_threshold(char *const cfgline, char *saveptr)
{
        uintmax_t tmp = 0;
        unsigned wait_threshold = 0;
        char *endptr;
        char *threshold_token = strtok_r(NULL, CFG_WHITESPACE, &saveptr);

        errno = 0;
        tmp = strtoumax(threshold_token, &endptr, WAIT_THRESHOLD_BASE);
        if (0 == tmp && threshold_token == endptr) {
                eprintf("strtoumax() : "
                        "the line does not contain a valid wait threshold\n");
                exit(EXIT_FAILURE);
        }
        /*checks overflow error*/
        if (UINTMAX_MAX == tmp && ERANGE == errno) {
                perror("strtoumax()");
                exit(EXIT_FAILURE);
        }

        wait_threshold = tmp;
        if (wait_threshold != tmp) {
                eprintf("strtoumax() : "
                        "number of seconds to wait "
                        "exceeds the capacity of an "
                        "unsigned integer\n");
                exit(EXIT_FAILURE);
        }
        return wait_threshold;
}

static char *config_parse_pname(char *const cfgline, char **saveptr)
{
        char *pname_token = strtok_r(cfgline, CFG_WHITESPACE, saveptr);

        if (NULL == pname_token) {
                eprintf("strtok_r() : "
                        "the line does not contain a valid process name\n");
                exit(EXIT_FAILURE);
        }

        return pname_token;
}
