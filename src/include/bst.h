#ifndef BST_H_
#define BST_H_

/*
 *Since part III the actual definition of bst_node_ is moved to this public
 *header to avoid link time errors since both the client and server rely
 *on bst; it is better to keep the definition of type specific bst functions
 *into separate source files.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*
 *No typedef for struct bst_node_ to avoid collision with other headers:
 *noteably link in <unistd.h>.
 */
enum bst_link_dir { BST_LEFT, BST_RIGHT, BST_LINKSIZE };

struct bst_node_ {
        long key;
        void *memblk;
        /*
         *Unlike the implementation of castack, here I use 2 separate fields
         *to store the size of the memory block.
         */
        size_t blknum;  /* number of blocks */
        size_t blksize; /* size of each block */
        struct bst_node_ *link[BST_LINKSIZE]; /* child pointer */
};

struct bst {
        struct bst_node_ *root;
        size_t numnode;
};

enum bst_type {
        PW_CLIENT_BST,
        PW_PID_BST,
        PW_IDLE_BST,
};

struct bst *bst_init(void)
        __attribute__((warn_unused_result));
void *bst_find(struct bst *current_bst, long key)
        __attribute__((warn_unused_result));
void *bst_add(struct bst *current_bst, long key, size_t blknum, size_t blksize)
        __attribute__((warn_unused_result));
long bst_rootkey(struct bst *current_bst);
int bst_del(struct bst *current_bst, long key);
int bst_destroy(struct bst **current_bst, enum bst_type type);

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
