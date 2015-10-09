#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>
#include <sys/types.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

struct pw_config_info {
    enum { PW_WAIT_THRESHOLD, PW_PROCESS_NAME, PW_END_FILE } type;
    union {
        unsigned   wait_threshold;
        const char *process_name;
    } data;
};

struct pw_watched_pid_info {
    pid_t child_pid;
    pid_t watched_pid;
    char *process_name;
};


struct pw_log_info {
    enum { INFO_INIT, INFO_NOEXIST, INFO_REPORT, ACTION_KILL } log_type;
    pid_t watched_pid;
    unsigned   wait_threshold;
    const char *process_name;
    size_t num_term;
};

void procwatch(int argc, char **argv);

#endif
