#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>
#include <sys/types.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

enum { PW_LINEBUF_SIZE = 1024 };

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

void procwatch(int argc, char **argv);

#endif
