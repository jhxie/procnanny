#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <features.h>
/*secure_getenv first appeared in glibc 2.17*/
#if !(__GLIBC__ == 2 && __GLIBC_MINOR__ >= 17)
#error "This software requires the glibc to be at least 2.17"
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "pwwrapper.h"

#include "memwatch.h"

int procclean(enum pw_clean_type clean_type)
{
        const char *clean_pipe_str = NULL;

        switch (clean_type) {
        case PW_CLIENT_CLEAN:
#ifdef NDEBUG
                clean_pipe_str = "pidof -x procnanny.client | tr \' \' \'\n\'";
#else
                clean_pipe_str = "pidof -x procnanny.client_debug"
                        " | tr \' \' \'\n\'";
#endif
                break;
        case PW_SERVER_CLEAN:
#ifdef NDEBUG
                clean_pipe_str = "pidof -x procnanny.server | tr \' \' \'\n\'";
#else
                clean_pipe_str = "pidof -x procnanny.server_debug"
                        " | tr \' \' \'\n\'";
#endif
                break;
        default:
                errno = EINVAL;
                return -1;
        }
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         */
        FILE *clean_pipe              = popen_or_die(clean_pipe_str, "r");
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr                  = NULL;
        const pid_t pw_id             = getpid();
        uintmax_t zombie_id           = 0;
        errno                         = 0;

        while (NULL != fgets(linebuf, sizeof linebuf, clean_pipe)) {
                zombie_id = strtoumax(linebuf, &endptr, 10);

                if (pw_id == (pid_t)zombie_id)
                        continue;

                if (0 != kill((pid_t)zombie_id, SIGKILL)) {
                        perror("kill()");
                        exit(EXIT_FAILURE);
                }
        }
        pclose_or_die(clean_pipe);
        return 0;
}

/*
 *Based on the "BSD Sockets: A Quick And Dirty Primer" by Jim Frost
 */
void data_write(int fd, const void *buf, size_t n)
{
        size_t bcount = 0; /* counts bytes write */
        size_t br     = 0; /* bytes write this pass */
        /* loop until full buffer */
        while (bcount < n) {
                if (0 < (br = write_or_die(fd, buf, n - bcount))) {
                        bcount += br; /* increment byte counter */
                        buf += br; /* move buffer ptr for next write */
                }
        }
}

/*
 *Based on the "BSD Sockets: A Quick And Dirty Primer" by Jim Frost
 */
void data_read(int fd, void *buf, size_t n)
{
        size_t bcount = 0; /* counts bytes write */
        size_t br     = 0; /* bytes write this pass */
        /* loop until full buffer */
        while (bcount < n) {
                if (0 < (br = read_or_die(fd, buf, n - bcount))) {
                        bcount += br; /* increment byte counter */
                        buf += br; /* move buffer ptr for next write */
                }
        }
}

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

void pipe_or_die(int *pipefd)
{
        if (-1 == pipe(pipefd)) {
                perror("pipe()");
                exit(EXIT_FAILURE);
        }
}

ssize_t write_or_die(int fd, const void *buf, size_t n)
{
        ssize_t val;
        if (-1 == (val = write(fd, buf, n))) {
                perror("write()");
                exit(EXIT_FAILURE);
        }
        return val;
}

ssize_t read_or_die(int fd, void *buf, size_t n)
{
        ssize_t val;
        if (-1 == (val = read(fd, buf, n))) {
                perror("read()");
                exit(EXIT_FAILURE);
        }
        return val;
}

void sigdelset_or_die(sigset_t *set, int signo)
{
        if (-1 == sigdelset(set, signo)) {
                perror("sigdelset()");
                exit(EXIT_FAILURE);
        }
}

void sigfillset_or_die(sigset_t *set)
{
        if (-1 == sigfillset(set)) {
                perror("sigfillset()");
                exit(EXIT_FAILURE);
        }
}

void sigprocmask_or_die(int how, const sigset_t *set, sigset_t *oldset)
{
        if (-1 == sigprocmask(how, set, oldset)) {
                perror("sigprocmask()");
                exit(EXIT_FAILURE);
        }
}

void sigaction_or_die(int sig,
                      const struct sigaction *act,
                      struct sigaction *oldact)
{
        if (-1 == sigaction(sig, act, oldact)) {
                perror("sigaction()");
                exit(EXIT_FAILURE);
        }
}

void sigemptyset_or_die(sigset_t *set)
{
        if (-1 == sigemptyset(set)) {
                perror("sigemptyset()");
                exit(EXIT_FAILURE);
        }
}

int socket_or_die(int domain, int type, int protocol)
{
        int sockdes;
        if (-1 == (sockdes = socket(domain, type, protocol))) {
                perror("socket()");
                exit(EXIT_FAILURE);
        }
        return sockdes;
}

void bind_or_die(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
        if (-1 == bind(sockfd, addr, addrlen)) {
                perror("bind()");
                exit(EXIT_FAILURE);
        }
}

void listen_or_die(int sockfd, int backlog)
{
        if (-1 == listen(sockfd, backlog)) {
                perror("listen()");
                exit(EXIT_FAILURE);
        }
}

void gethostname_or_die(char *name, size_t len)
{
        if (-1 == gethostname(name, len)) {
                perror("gethostname()");
                exit(EXIT_FAILURE);
        }
}

void getnameinfo_or_die(const struct sockaddr *sa, socklen_t salen,
			char *host, socklen_t hostlen,
			char *serv, socklen_t servlen,
			int flags)
{
        int ecode = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
        if (0 != ecode) {
                errx(EXIT_FAILURE, "getnameinfo(): %s", gai_strerror(ecode));
        }
}
