#ifndef PW_WRAPPER_H_
#define PW_WRAPPER_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/*For socket() bind()*/
#include <sys/types.h>
#include <sys/socket.h>

/*
 *Based on the variadic macro expansion example from
 *https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
 *Not used since part II -- 4.4BSD series of err functions
 *are better alternatives
 */
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
/*
 *zerofree
 *set the pointer to NULL after free
 */
#define zerofree(ptr) \
        do { \
                free(ptr); \
                ptr = NULL; \
        } while (0)

/*
 *According to part III specification,
 *"For this assignment, you can safely assume that there will never be more than
 *32 clients"
 */
enum {
        PW_SERVER_PORT_NUM    = 9898,
        PW_SERVER_MAX_BACKLOG = 32,
        PW_LINEBUF_SIZE       = 1024,
};

enum pw_clean_type {
        PW_CLIENT_CLEAN,
        PW_SERVER_CLEAN,
};

struct pw_pid_info {
        enum {
                INFO_STARTUP, INFO_INIT, INFO_NOEXIST,
                INFO_REPORT, INFO_REREAD, ACTION_KILL
        } type;
        pid_t watched_pid;
        pid_t child_pid;
        unsigned wait_threshold;
        char process_name[PW_LINEBUF_SIZE];
        int ipc_fdes[2];
};

int procclean(enum pw_clean_type clean_type);
void data_write(int fd, const void *buf, size_t n);
void data_read(int fd, void *buf, size_t n);

      /*Below are all wrapper functions for certain library/system calls*/
void *calloc_or_die(size_t nmemb, size_t size);
void *realloc_or_die(void *const ptr, size_t size);
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
int fputs_or_die(const char *string, FILE *stream);
void pipe_or_die(int *pipefd);
ssize_t write_or_die(int fd, const void *buf, size_t n);
ssize_t read_or_die(int fd, void *buf, size_t n);
void sigfillset_or_die(sigset_t *set);
void sigdelset_or_die(sigset_t *set, int signo);
void sigprocmask_or_die(int how, const sigset_t *set, sigset_t *oldset);
void sigaction_or_die(int sig,
                      const struct sigaction *act,
                      struct sigaction *oldact);
void sigemptyset_or_die(sigset_t *set);
int socket_or_die(int domain, int type, int protocol);
void bind_or_die(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
void listen_or_die(int sockfd, int backlog);
void gethostname_or_die(char *name, size_t len);
void getnameinfo_or_die(const struct sockaddr *sa, socklen_t salen,
			char *host, socklen_t hostlen,
			char *serv, socklen_t servlen,
			int flags);
      /*Above are all wrapper functions for certain library/system calls*/
#endif
