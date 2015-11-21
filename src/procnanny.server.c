#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "cfgparser.h"
#include "pwlog.h"
#include "pwwrapper.h"

#define PWSERVER_ONLY
#include "pwsutils.h"
#undef PWSERVER_ONLY

#include "memwatch.h"

const char *configname                    = NULL;
size_t num_killed                         = 0;
static FILE *pwlog                        = NULL;
static volatile sig_atomic_t sig_hup_flag = false;
static volatile sig_atomic_t sig_int_flag = false;

int main(int argc, char *argv[])
{
        if (2 != argc) {
                errx(EXIT_FAILURE, "Usage: %s [CONFIGURATION FILE]", argv[0]);
        }

        configname = argv[1];
        struct sigaction sa = {
                .sa_handler = signal_handle,
                /*.sa_flags = SA_RESTART,*/
        };
        sigemptyset_or_die(&sa.sa_mask);
        sigaction_or_die(SIGINT, &sa, NULL);
        sigaction_or_die(SIGHUP, &sa, NULL);
        setbuf(stdout, NULL); /*Make stdout unbuffered.*/
        procclean();
        procserver();

        return EXIT_SUCCESS;
}

static void procserver(void)
{
        pwlog = pwlog_setup();
        size_t numcfgline = config_parse(configname);
        int listen_sockfd = socket_or_die(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_sockaddr = {
                .sin_family = AF_INET,
                .sin_addr.s_addr = INADDR_ANY, /*bind() to all local interface*/ 
                .sin_port = htons(PW_SERVER_PORT_NUM)
        };
        fd_set readset;
        FD_ZERO(&readset);
        FD_SET(listen_sockfd, &readset);

        /*Write to the environment variable PROCNANNYSERVERINFO*/
        pwlog_write(pwlog, &((struct pw_pid_info){
                             .type = INFO_STARTUP}));
        bind_or_die(listen_sockfd,
                    (struct sockaddr *)&server_sockaddr,
                    sizeof server_sockaddr);
        listen_or_die(listen_sockfd, PW_SERVER_MAX_BACKLOG);

        for (fd_set tmpset = readset; true;
             client_socklen = sizeof client_sockaddr) {

                client_sockfd = accept(listen_sockfd,
                                   (struct sockaddr *)&client_sockaddr,
                                   &client_socklen);
                if (0 > client_sockfd) {
                        perror("accept()");
                        exit(EXIT_FAILURE);
                }
                /*write_or_die(client_sockfd, &MAGIC, sizeof MAGIC);*/
        }
        close_or_die(listen_sockfd);
        fclose_or_die(pwlog);
}

static void descriptors_inspect(const int listen_sockfd, const fd_set *readset)
{
        fd_set tmpset = *readset;
        struct timespec ts = {
                .tv_sec = 0,
                .tv_nsec = 0
        };
        sigset_t mask;
        sigfillset_or_die(&mask);
        switch (pselect(getdtablesize(), &tmpset, NULL, NULL, &ts, &mask)) {
        case -1:
                perror("pselect()");
                exit(EXIT_FAILURE);
        case 0: /*No descriptors are available for reading*/ 
                return;
        default:
                if (FD_ISSET(listen_sockfd, &tmpset)) {
                        clients_serve(listen_sockfd);
                }
                batchlog_write(&tmpset);
        }
}

static void clients_serve(const int listen_sockfd)
{
        struct sockaddr_in client_sockaddr = { 0 };
        socklen_t client_socklen = sizeof client_sockaddr;
        int client_sockfd = accept(listen_sockfd,
                                   (struct sockaddr *)&client_sockaddr,
                                   &client_socklen);
}


static void batchlog_write(const fd_set *clientset);
static void signal_handle(int sig)
{
        switch (sig) {
        case SIGHUP:
                sig_hup_flag = true;
                break;
        case SIGINT:
                sig_int_flag = true;
                break;
        }
}
