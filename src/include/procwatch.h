#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>
#include <sys/types.h>

struct pw_watched_pid_info {
    pid_t child_pid;
    pid_t watched_pid;
    char *process_name;
};

void procwatch(int argc, char **argv);

#endif
