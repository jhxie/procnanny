#ifndef BSTCLIENT_H_
#define BSTCLIENT_H_

#include <sys/types.h>

#include "bst.h"

/*Used for all the clients to communicate with their children processes*/
int pw_pid_bst_refresh(struct bst *pw_pid_bst,
                       struct bst *pw_idle_bst,
                       const fd_set *tmpset,
                       const int server_sockfd);
/*Used for all the clients to communicate with their children processes*/

#endif
