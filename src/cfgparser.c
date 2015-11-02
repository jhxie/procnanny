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

static const char *const CFG_WHITESPACE = "\t ";
static const int WAIT_THRESHOLD_BASE = 10;

void config_parse(struct pw_log_info *const pwlog, char *const cfgline)
{
        /*
         *Note here the saveptr is used for strtok_r to recover the context
         *left off by the first call; which occurred in config_parse_pname().
         */
        char *saveptr = NULL;
        pwlog->process_name = config_parse_pname(cfgline, &saveptr);
        pwlog->wait_threshold = config_parse_threshold(cfgline, saveptr);
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
