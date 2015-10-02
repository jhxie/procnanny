#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

void procwatch(int argc, char **argv);
FILE *fopen_or_die(const char *path, const char *mode);
void fclose_or_die(FILE *stream);
void fgetpos_or_die(FILE *stream, fpos_t *pos);
void fsetpos_or_die(FILE *stream, const fpos_t *pos);

#endif
