#ifndef CFGPARSER_H_
#define CFGPARSER_H_

#include "pwlog.h"

/*
 *Based on ASSUMPTION #4
 */
enum { PW_CFG_MAX_LEN_PROGRAM_NAME = 1024 };

struct pw_cfg_info {
        char process_name[PW_CFG_MAX_LEN_PROGRAM_NAME];
        unsigned wait_threshold;
};

/*
 *From the assignment specification
 */
enum { PW_CFG_MAX_NUM_PROGRAM_NAME = 128 };

extern struct pw_cfg_info pw_cfg_vector[PW_CFG_MAX_NUM_PROGRAM_NAME];

size_t config_parse(const char *const cfgname);
#endif
