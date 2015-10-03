#ifndef PW_WRAPPER_H_
#define PW_WRAPPER_H_

#include <stdio.h>

FILE *fopen_or_die(const char *path, const char *mode);
void fclose_or_die(FILE *stream);
void fgetpos_or_die(FILE *stream, fpos_t *pos);
void fsetpos_or_die(FILE *stream, const fpos_t *pos);
#endif
