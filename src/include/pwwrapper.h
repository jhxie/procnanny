#ifndef PW_WRAPPER_H_
#define PW_WRAPPER_H_

#include <stdio.h>
#include <unistd.h>

FILE *fopen_or_die(const char *path, const char *mode);
void fclose_or_die(FILE *stream);
void fgetpos_or_die(FILE *stream, fpos_t *pos);
void fsetpos_or_die(FILE *stream, const fpos_t *pos);
FILE *popen_or_die(const char *command, const char *type);
void pclose_or_die(FILE *stream);
pid_t fork_or_die(void);
const char *secure_getenv_or_die(const char *name);
int open_or_die(const char *pathname, int flags);
void close_or_die(int fd);
#endif
