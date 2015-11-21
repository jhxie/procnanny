#if !defined (PWSUTILS_H_) && defined (PWSERVER_ONLY)
#define PWSUTILS_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>
#include <netdb.h>

struct pw_client_info {
        int  sockfd;
        char hostname[NI_MAXHOST];
        size_t killcnt;
};

static void procserver(void);
static void fdset_check(const int listen_sockfd, fd_set *readset);
static void clients_serve(const int listen_sockfd, fd_set *readset);
static void batchlog_write(const int totnumdes, const fd_set *clientset);
static void signal_handle(int sig);
static void clean_up(void);
#endif
