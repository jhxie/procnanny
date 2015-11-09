#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>
#include <sys/types.h>

#include "pwwrapper.h"

struct pw_watched_pid_info {
    pid_t watched_pid;
    pid_t child_pid;
    unsigned cwait_threshold;
    unsigned pwait_threshold;
    char process_name[PW_LINEBUF_SIZE];
    int ipc_fdes[2];
};

void procwatch(const char *const cfgname);

#endif
