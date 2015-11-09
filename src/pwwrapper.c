#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <features.h>
/*secure_getenv first appeared in glibc 2.17*/
#if !(__GLIBC__ == 2 && __GLIBC_MINOR__ >= 17)
#error "This software requires the glibc to be at least 2.17"
#endif

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "procwatch.h"
#include "pwwrapper.h"

#include "memwatch.h"

void *calloc_or_die(size_t nmemb, size_t size)
{
        void *rtn_ptr = calloc(nmemb, size);

        if (rtn_ptr == NULL) {
                perror("calloc()");
                exit(EXIT_FAILURE);
        } else {
                return rtn_ptr;
        }
}

void *realloc_or_die(void *const ptr, size_t size)
{
        void *rtn_ptr = realloc(ptr, size);

        if (rtn_ptr == NULL) {
                free(ptr);
                perror("realloc()");
                exit(EXIT_FAILURE);
        } else {
                return rtn_ptr;
        }
}

FILE *fopen_or_die(const char *path, const char *mode)
{
        FILE *file = fopen(path, mode);
        if (NULL == file) {
                perror("fopen()");
                exit(EXIT_FAILURE);
        }
        return file;
}

void fclose_or_die(FILE *stream)
{
        if (EOF == fclose(stream)) {
                perror("fclose()");
                exit(EXIT_FAILURE);
        }
}

void fgetpos_or_die(FILE *stream, fpos_t *pos)
{
        if (0 != fgetpos(stream, pos)) {
                perror("fgetpos()");
                exit(EXIT_FAILURE);
        }
}

void fsetpos_or_die(FILE *stream, const fpos_t *pos)
{
        if (0 != fsetpos(stream, pos)) {
                perror("fsetpos()");
                exit(EXIT_FAILURE);
        }
}

FILE *popen_or_die(const char *command, const char *type)
{
        FILE *file = popen(command, type);
        /*
         *According to the manual page,
         *"The popen() function does not set errno if memory allocation fails."
         *so perror() may not display error correctly.
         */
        if (NULL == file) {
                perror("popen()");
                exit(EXIT_FAILURE);
        }
        return file;
}

void pclose_or_die(FILE *stream)
{
        if (-1 == pclose(stream)) {
                perror("pclose()");
                exit(EXIT_FAILURE);
        }
}

pid_t fork_or_die(void)
{
        pid_t process_id = fork();

        if (-1 == process_id) {
                perror("fork()");
                exit(EXIT_FAILURE);
        } else {
                return process_id;
        }
}

const char *secure_getenv_or_die(const char *name)
{
        const char *val;
        if (NULL == (val = secure_getenv(name))) {
                errx(EXIT_FAILURE,
                     "The environment variable %s does not exist", name);
        }
        return val;
}

int open_or_die(const char *pathname, int flags)
{
        int fd;
        if (-1 == (fd = open(pathname, flags))) {
                perror("open()");
                exit(EXIT_FAILURE);
        }
        return fd;
}

void close_or_die(int fd)
{
        if (-1 == close(fd)) {
                perror("close()");
                exit(EXIT_FAILURE);
        }
}

int fputs_or_die(const char *string, FILE *stream)
{
        int val;
        if (EOF == (val = fputs(string, stream))) {
                errx(EXIT_FAILURE, "fputs() : write failed");
        }
        return val;
}
