#ifndef BST_H_
#define BST_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/select.h>

/*
 *No typedef for struct bst_node_ to avoid collision with other headers:
 *noteably link in <unistd.h>.
 */
struct bst {
        struct bst_node_ *root;
        size_t numnode;
};

struct bst *bst_init(void)
        __attribute__((warn_unused_result));
void *bst_find(struct bst *current_bst, long key)
        __attribute__((warn_unused_result));
void *bst_add(struct bst *current_bst, long key, size_t blknum, size_t blksize)
        __attribute__((warn_unused_result));
long bst_rootkey(struct bst *current_bst);
int bst_del(struct bst *current_bst, long key);
int bst_destroy(struct bst **current_bst);


/*Used for all the clients to communicate with their children processes*/
int pw_pid_bst_interval_add(struct bst *current_bst, unsigned ival);
int pw_pid_bst_refresh(struct bst *pw_pid_bst,
                       struct bst *pw_idle_bst,
                       FILE *pwlog);
/*Used for all the clients to communicate with their children processes*/


/*Used for the server to communicate with all its clients*/
int pw_client_bst_batchlog(struct bst *pw_client_bst,
                           const fd_set *pw_clientset,
                           FILE *pwlog);
int pw_client_bst_report(struct bst *pw_client_bst, FILE *pwlog);
int pw_client_bst_batchsend(struct bst *pw_client_bst, FILE *pwlog);
/*Used for the server to communicate with all its clients*/


static inline bool bst_isempty(struct bst *current_bst)
        __attribute__((always_inline));
static inline size_t bst_report(struct bst *current_bst)
        __attribute__((always_inline));

static inline bool bst_isempty(struct bst *current_bst)
{
        return current_bst->root == NULL;
}

static inline size_t bst_report(struct bst *current_bst)
{
        return current_bst->numnode;
}
#endif
