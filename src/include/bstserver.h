#ifndef BSTSERVER_H_
#define BSTSERVER_H_

#include <sys/types.h>

#include "bst.h"
/*Used for the server to communicate with all its clients*/
int pw_client_bst_batchlog(struct bst *pw_client_bst,
                           const fd_set *pw_clientset,
                           FILE *pwlog);
int pw_client_bst_report(struct bst *pw_client_bst, FILE *pwlog);
int pw_client_bst_batchsend(struct bst *pw_client_bst, FILE *pwlog);
/*Used for the server to communicate with all its clients*/

#endif
