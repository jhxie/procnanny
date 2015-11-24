#include <err.h>
#include <errno.h>
#include <netdb.h> /*definition for getnameinfo*/
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "bst.h"
#include "cfgparser.h"
#include "procserver.h"
#include "pwlog.h"
#include "pwversion.h"
#include "pwwrapper.h"

#define PWSERVER_ONLY
#include "pwsutils.h"
#undef PWSERVER_ONLY

#include "memwatch.h"

const char *configname                    = NULL;
size_t num_killed                         = 0;
static struct bst *pw_client_bst          = NULL;
static FILE *pwlog                        = NULL;
static volatile sig_atomic_t sig_hup_flag = false;
static volatile sig_atomic_t sig_int_flag = false;

int main(int argc, char *argv[])
{
        if (2 != argc) {
                errx(EXIT_FAILURE, "Usage: %s [CONFIGURATION FILE]", argv[0]);
        }

        pw_client_bst = bst_init();

        if (0 != atexit(clean_up)) {
                bst_destroy(&pw_client_bst, (enum bst_type)PW_CLIENT_BST);
                errx(EXIT_FAILURE, "atexit() : failed to register clean_up()");
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
        memset(pw_cfg_vector, 0, sizeof pw_cfg_vector);
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

        bind_or_die(listen_sockfd,
                    (struct sockaddr *)&server_sockaddr,
                    sizeof server_sockaddr);
        listen_or_die(listen_sockfd, PW_SERVER_MAX_BACKLOG);
        /*Write to the environment variable PROCNANNYSERVERINFO*/
        pwlog_write(pwlog, &((struct pw_pid_info){
                             .type = INFO_STARTUP}), NULL);

        while (true) {
                fdset_check(listen_sockfd, &readset);
                if (true == sig_int_flag) {
                        sig_int_flag = false;
                        pw_client_bst_report(pw_client_bst, pwlog);
                        break;
                }
                if (true == sig_hup_flag) {
                        sig_hup_flag = false;
                        memset(pw_cfg_vector, 0, sizeof pw_cfg_vector);
                        numcfgline = config_parse(configname);
                        pw_client_bst_batchsend(pw_client_bst,pwlog);
                }
        }

        close_or_die(listen_sockfd);
        fclose_or_die(pwlog);
}

static void fdset_check(const int listen_sockfd, fd_set *readset)
{
        /*
         *This temporary file descriptor set is used to feed pselect():
         *pselect() call would modify the content of the supplied file
         *descriptor set.
         */
        fd_set tmpset = *readset;
        sigset_t mask;
        int numdes;
        sigfillset_or_die(&mask);
        sigdelset_or_die(&mask, SIGINT);
        sigdelset_or_die(&mask, SIGHUP);
        /*
         *Let the pselect() block indefinitely as long as there is
         *no new incoming connection request or struct pw_pid_info from
         *already connected clients;
         *Except the case that the SIGINT or SIGHUP signal is delivered,
         *which would interrupt the pselect() systemcall, so a restart
         *is required.
         */
        do {
                errno = 0;
                numdes = pselect(getdtablesize(), &tmpset,
                                 NULL, NULL, NULL, &mask);

                if (-1 == numdes && EINTR != errno) {
                        perror("pselect()");
                        exit(EXIT_FAILURE);
                }
        } while (-1 == numdes && EINTR == errno);

        switch (numdes) {
        case 0: /*No descriptors are available for reading*/ 
                return;
        default:
                if (FD_ISSET(listen_sockfd, &tmpset)) {
                        /*
                         *Note that readset is ONLY MODIFIED if there is a
                         *new connection queueing to be served.
                         */
                        clients_serve(listen_sockfd, readset);
                        /*
                         *Return directly if the listening socket is the only
                         *descriptor available for reading:
                         *there is no need to call batchlog_write()
                         */
                        if (1 == numdes) {
                                return;
                        }
                        /*
                         *Excluding the listening socket itself
                         *from the batchlog_write()
                         */
                        FD_CLR(listen_sockfd, &tmpset);
                        numdes--;
                }
                pw_client_bst_batchlog(pw_client_bst, &tmpset, pwlog);
        }
}

static void clients_serve(const int listen_sockfd, fd_set *readset)
{
        struct sockaddr_in client_sockaddr = { 0 };
        socklen_t client_socklen = sizeof client_sockaddr;
        int client_sockfd = accept(listen_sockfd,
                                   (struct sockaddr *)&client_sockaddr,
                                   &client_socklen);
        /*
         *Add the newly connected client to the readset so future
         *pselect() call would detect their existence
         */
        FD_SET(client_sockfd, readset);
        struct pw_client_info *client_ptr =
                bst_add(pw_client_bst,
                        client_sockfd,
                        1,
                        sizeof(struct pw_client_info));
        getnameinfo_or_die((struct sockaddr *)&client_sockaddr, client_socklen,
                           client_ptr->hostname, NI_MAXHOST,
                           NULL, 0, NI_NAMEREQD);
        client_ptr->sockfd  = client_sockfd;
        client_ptr->killcnt = 0;
        /*Send the configuration file vector to the newly connected client*/
        data_write(client_sockfd, pw_cfg_vector, sizeof pw_cfg_vector);
}

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

static void clean_up(void)
{
        bst_destroy(&pw_client_bst, (enum bst_type)PW_CLIENT_BST);
}
