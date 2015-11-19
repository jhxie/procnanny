#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "pwwrapper.h"

#include "memwatch.h"

int main(int argc, char *argv[])
{
        int MAGIC;
        if (3 != argc || strlen(argv[2]) != strspn(argv[2], "0123456789")) {
                errx(EXIT_FAILURE,
                     "Usage: %s [HOST NAME] [PORT NUMBER]",
                     argv[0]);
        }

        struct addrinfo server_info = {
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
                .ai_flags = 0,
                .ai_protocol = 0
        };
        struct addrinfo *host_list      = NULL;
        struct addrinfo *host_list_iter = NULL;
        int server_sockfd;
        int gai_return_val = getaddrinfo(argv[1], argv[2],
                                         &server_info, &host_list);
        if (0 != gai_return_val) {
                errx(EXIT_FAILURE,
                     "getaddrinfo(): %s",
                     gai_strerror(gai_return_val));
        }

        for (host_list_iter = host_list;
             NULL != host_list_iter;
             host_list_iter = host_list_iter->ai_next) {
                server_sockfd = socket(host_list_iter->ai_family,
                                       host_list_iter->ai_socktype,
                                       host_list_iter->ai_protocol);
                if (-1 != connect(server_sockfd,
                                  host_list_iter->ai_addr,
                                  host_list_iter->ai_addrlen)) {
                        break;
                }
                close_or_die(server_sockfd);
        }

        if (NULL == host_list_iter) {
                errx(EXIT_FAILURE,
                     "Host %s with Port Number %s is not reachable",
                     argv[1],
                     argv[2]);
        }
        freeaddrinfo(host_list);
        host_list = NULL;
        read_or_die(server_sockfd, &MAGIC, sizeof MAGIC);
        printf("%d\n", ntohl(MAGIC));
        puts("reached");
        close_or_die(server_sockfd);
        return EXIT_SUCCESS;
}
