#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

struct pw_config_info {
    enum { PW_WAIT_THRESHOLD, PW_PROCESS_NAME, PW_END_FILE } type;
    union {
        unsigned   wait_threshold;
        const char *process_name;
    } data;
};

void procwatch(int argc, char **argv);
FILE *fopen_or_die(const char *path, const char *mode);
void fclose_or_die(FILE *stream);
void fgetpos_or_die(FILE *stream, fpos_t *pos);
void fsetpos_or_die(FILE *stream, const fpos_t *pos);

#endif
