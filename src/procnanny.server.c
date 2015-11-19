#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "pwwrapper.h"

#include "memwatch.h"

int main(int argc, char *argv[])
{
        const int MAGIC = htonl(64);
        int client_sockfd;
        int listen_sockfd = socket_or_die(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in client_sockaddr = { 0 };
        struct sockaddr_in server_sockaddr = {
                .sin_family = AF_INET,
                .sin_addr.s_addr = INADDR_ANY,
                .sin_port = htons(PW_SERVER_PORT_NUM)
        };
        /*
         *When INADDR_ANY is specified in the bind call, the socket will be
         *bound to all local interfaces.
         */
        socklen_t client_socklen;

        bind_or_die(listen_sockfd,
                    (struct sockaddr *)&server_sockaddr,
                    sizeof server_sockaddr);
        listen_or_die(listen_sockfd, PW_SERVER_MAX_BACKLOG);

        for (client_socklen = sizeof client_sockaddr;
             true;
             client_socklen = sizeof client_sockaddr) {

                client_sockfd = accept(listen_sockfd,
                                   (struct sockaddr *)&client_sockaddr,
                                   &client_socklen);
                if (0 > client_sockfd) {
                        perror("accept()");
                        exit(EXIT_FAILURE);
                }
                write_or_die(client_sockfd, &MAGIC, sizeof MAGIC);
                close_or_die(client_sockfd);
        }
        close_or_die(listen_sockfd);
        return EXIT_SUCCESS;
}
