#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>
#include <sys/types.h>

struct pw_watched_pid_info {
    pid_t child_pid;
    pid_t watched_pid;
    unsigned cwait_threshold;
    unsigned pwait_threshold;
    int ipc_fdes[2];
};

void procwatch(const char *const cfgname);

#endif
