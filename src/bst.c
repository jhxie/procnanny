/*
 *Note: this implementation is mostly based on the code examples from
 *http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_bst1.aspx
 *the code is modified extensively to suit the need of procnanny.
 */
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h> /*MAX_SIZE definition*/

#ifdef BST_DEBUG
#include <string.h>
#include <unistd.h>
#endif

#include "bst.h"
#include "procwatch.h"
#include "pwwrapper.h"

#include "memwatch.h"

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
        struct bst_node_ *up; /* parent pointer */
        struct bst_node_ *link[BST_LINKSIZE]; /* child pointer */
};

struct bst *bst_init(void)
{
        struct bst *new_bst = calloc_or_die(1, sizeof(struct bst));

        /*redundant but logical*/
        new_bst->numnode = 0;
        new_bst->root = NULL;

        return new_bst;
}

void *bst_find(struct bst *current_bst, long key)
{
        /*
         *avoid undefined behavior by returning directly
         *rather than trying to dereference a NULL pointer
         */
        if (NULL == current_bst) {
                errno = EINVAL;
                return NULL;
        }

        enum bst_link_dir dir    = BST_LEFT;
        struct bst_node_ *tmp_ptr = current_bst->root;

        while (NULL != tmp_ptr) {
                if (key == tmp_ptr->key) {
                        break;
                } else {
                        dir     = key > tmp_ptr->key;
                        tmp_ptr = tmp_ptr->link[dir];
                }
        }

        return (NULL == tmp_ptr) ? NULL : tmp_ptr->memblk;
}

void *bst_add(struct bst *current_bst, long key, size_t blknum, size_t blksize)
{
        if (NULL == current_bst) {
                errno = EINVAL;
                return NULL;
        }

        struct bst_node_ *bstnode = calloc_or_die(1, sizeof(struct bst_node_));

        if (current_bst->numnode + 1 == SIZE_MAX) {
#ifdef BST_DEBUG
                const char *const err_msg = "bst: max node count reached\n";
                write(STDERR_FILENO, err_msg, strlen(err_msg));
#endif
                free(bstnode);
                errno = EOVERFLOW;
                return NULL;
        }

        bstnode->key                        = key;
        bstnode->memblk                     = calloc_or_die(blknum, blksize);
        bstnode->blknum                     = blknum;
        bstnode->blksize                    = blksize;
        /*again, redundant*/
        bstnode->up              = NULL;
        bstnode->link[BST_LEFT]  = NULL;
        bstnode->link[BST_RIGHT] = NULL;
        
        /*
         *start to insert the new node into the bst
         */
        if (NULL == current_bst->root) {
                current_bst->root = bstnode;
        } else {
                enum bst_link_dir dir    = BST_LEFT;
                struct bst_node_ *tmp_ptr = current_bst->root;

                while (true) {
                        /*note that duplicate keys are NOT allowed*/
                        if (bstnode->key == tmp_ptr->key) {
                                free(bstnode);
                                return NULL;
                        }

                        dir = bstnode->key > tmp_ptr->key;

                        if (NULL == tmp_ptr->link[dir])
                                break;

                        tmp_ptr = tmp_ptr->link[dir];
                }
                tmp_ptr->link[dir] = bstnode;
                tmp_ptr->link[dir]->up = tmp_ptr;
        }

        current_bst->numnode++;
        return bstnode->memblk;
}

long bst_rootkey(struct bst *current_bst)
{
        if (NULL == current_bst || NULL == current_bst->root) {
                errno = EINVAL;
                return -1;
        }
        return current_bst->root->key;
}

int bst_del(struct bst *current_bst, long key)
{
        if (NULL == current_bst) {
                errno = EINVAL;
                return -1;
        }

        if (NULL != current_bst->root) {
                struct bst_node_ head     = {};
                struct bst_node_ *tmp_ptr = &head;
                struct bst_node_ *f_ptr   = NULL;
                enum bst_link_dir dir     = BST_RIGHT;

                /*make the new node's right chain pointer point to the root*/
                tmp_ptr->link[BST_RIGHT] = current_bst->root;
                current_bst->root->up = &head;

                while (NULL != tmp_ptr->link[dir]) {
                        tmp_ptr = tmp_ptr->link[dir];
                        dir = key >= tmp_ptr->key;

                        if (key == tmp_ptr->key) {
                                f_ptr = tmp_ptr;
                        }
                }

                if (NULL != f_ptr) {
                        enum bst_link_dir dir = tmp_ptr->link[BST_LEFT] == NULL;
                        
                        f_ptr->key = tmp_ptr->key;
                        f_ptr->blknum = tmp_ptr->blknum;
                        f_ptr->blksize = tmp_ptr->blksize;
                        free(f_ptr->memblk);
                        f_ptr->memblk = tmp_ptr->memblk;

                        tmp_ptr->up->link
                                [tmp_ptr->up->link[BST_RIGHT] == tmp_ptr] =
                                tmp_ptr->link[dir];
                        if (NULL != tmp_ptr->link[dir]) {
                                tmp_ptr->link[dir]->up = tmp_ptr->up;
                        }

                        free(tmp_ptr);
                        current_bst->numnode--;
                }
                current_bst->root = head.link[BST_RIGHT];

                if (NULL != current_bst->root) {
                        current_bst->root->up = NULL;
                }
        }
        return 0;
}

void *bst_first(struct bst_trav *trav, struct bst *current_bst)
{
        if (NULL == trav || NULL == current_bst) {
                errno = EINVAL;
                return NULL;
        }

        trav->tmp_ptr = current_bst->root;

        if (NULL != trav->tmp_ptr) {
                while (NULL != trav->tmp_ptr->link[BST_LEFT]) {
                        trav->tmp_ptr = trav->tmp_ptr->link[BST_LEFT];
                }
        }

        if (NULL != trav->tmp_ptr) {
                return &trav->tmp_ptr->memblk;
        } else {
                return NULL;
        }
}

void *bst_next(struct bst_trav *trav)
{
        if (NULL == trav) {
                errno = EINVAL;
                return NULL;
        }

        if (NULL != trav->tmp_ptr->link[BST_RIGHT]) {
                trav->tmp_ptr = trav->tmp_ptr->link[BST_RIGHT];

                while (NULL != trav->tmp_ptr->link[BST_LEFT]) {
                        trav->tmp_ptr = trav->tmp_ptr->link[BST_LEFT];
                }
        } else {
                while (true) {
                        if (NULL == trav->tmp_ptr->up ||
                            trav->tmp_ptr == trav->tmp_ptr->up->link[BST_LEFT]){
                                trav->tmp_ptr = trav->tmp_ptr->up;
                                break;
                        }
                        trav->tmp_ptr = trav->tmp_ptr->up;
                }
        }

        if (NULL != trav->tmp_ptr) {
                return &trav->tmp_ptr->memblk;
        } else {
                return NULL;
        }
}

int bst_destroy(struct bst **current_bst)
{
        if (NULL == current_bst || NULL == *current_bst) {
                errno = EINVAL;
                return -1;
        }

        struct bst_node_ *tmp_ptr = (*current_bst)->root;
        struct bst_node_ *saveptr;

        while (NULL != tmp_ptr) {
                if (NULL != tmp_ptr->link[BST_LEFT]) {
                        saveptr = tmp_ptr->link[BST_LEFT];
                        tmp_ptr->link[BST_LEFT] = saveptr->link[BST_RIGHT];
                        saveptr->link[BST_RIGHT] = tmp_ptr;
                } else {
                        saveptr = tmp_ptr->link[BST_RIGHT];
                        free(tmp_ptr->memblk);
                        free(tmp_ptr);
                }
                tmp_ptr = saveptr;
        }
        free(*current_bst);
        *current_bst = NULL;
        return 0;
}

void pw_pid_bst_add_interval(struct bst *current_bst, unsigned interval)
{
        struct bst_trav trav;
        struct pw_pid_info *tmp_ptr = bst_first(&trav, current_bst);

        while (NULL != tmp_ptr) {
                /*
                 *if a furthur increment would result in overflow, set
                 *the parent wait threshold to UINT_MAX
                 */
                if ((unsigned long)tmp_ptr->pwait_threshold +
                    (unsigned long)interval >=
                    (unsigned long)UINT_MAX) {
                        tmp_ptr->pwait_threshold = UINT_MAX;
                } else {
                        tmp_ptr->pwait_threshold += interval;
                }
                tmp_ptr = bst_next(&trav);
        }
}
