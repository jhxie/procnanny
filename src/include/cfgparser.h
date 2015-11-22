#ifndef CFGPARSER_H_
#define CFGPARSER_H_

#include "pwlog.h"

enum {
        PW_CFG_MAX_LEN_PROGRAM_NAME = 1024, /*Based on assumption 4*/
        PW_CFG_MAX_NUM_PROGRAM_NAME = 128 /*From assignment specification*/
};

struct pw_cfg_info {
        char process_name[PW_CFG_MAX_LEN_PROGRAM_NAME];
        unsigned wait_threshold;
};

extern struct pw_cfg_info pw_cfg_vector[PW_CFG_MAX_NUM_PROGRAM_NAME];

size_t config_parse(const char *const cfgname);
#endif
