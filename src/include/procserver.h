#ifndef PROCSERVER_H_
#define PROCSERVER_H_

#include <stddef.h>
#include <netdb.h>

struct pw_client_info {
        int  sockfd;
        char hostname[NI_MAXHOST];
        size_t killcnt;
};

extern const char *configname;
extern size_t num_killed;
#endif
