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
#include <string.h>

#include "bst.h"
#include "cfgparser.h"
#include "procclient.h"
#include "procserver.h"
#include "pwlog.h"
#include "pwwrapper.h"

#include "memwatch.h"

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

        if (SIZE_MAX == current_bst->numnode) {
#ifndef NDEBUG
                const char *const err_msg = "bst: max node count reached\n";
                write(STDERR_FILENO, err_msg, strlen(err_msg));
#endif
                free(bstnode);
                errno = EOVERFLOW;
                return NULL;
        }

        bstnode->key             = key;
        bstnode->memblk          = calloc_or_die(blknum, blksize);
        bstnode->blknum          = blknum;
        bstnode->blksize         = blksize;
        /*again, redundant*/
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
                struct bst_node_ head                   = {};
                struct bst_node_ *tmp_ptr               = &head;
                struct bst_node_ *p_ptr, *f_ptr = NULL;
                enum bst_link_dir dir                   = BST_RIGHT;

                /*make the new node's right chain pointer point to the root*/
                tmp_ptr->link[BST_RIGHT] = current_bst->root;

                while (NULL != tmp_ptr->link[dir]) {
                        p_ptr = tmp_ptr;
                        tmp_ptr = tmp_ptr->link[dir];
                        dir = key >= tmp_ptr->key;

                        if (key == tmp_ptr->key) {
                                f_ptr = tmp_ptr;
                        }
                }

                if (NULL != f_ptr) {
                        f_ptr->key = tmp_ptr->key;
                        f_ptr->blknum = tmp_ptr->blknum;
                        f_ptr->blksize = tmp_ptr->blksize;
                        free(f_ptr->memblk);
                        f_ptr->memblk = tmp_ptr->memblk;
                        p_ptr->link[tmp_ptr == p_ptr->link[BST_RIGHT]] =
                                tmp_ptr->link[NULL == tmp_ptr->link[BST_LEFT]];
                        free(tmp_ptr);
                        current_bst->numnode--;
                }
                current_bst->root = head.link[BST_RIGHT];
        }
        return 0;
}

int bst_destroy(struct bst **current_bst, enum bst_type type)
{
        if (NULL == current_bst || NULL == *current_bst) {
                errno = EINVAL;
                return -1;
        }

        struct pw_pid_info *pid_info_ptr;
        struct pw_idle_info *idle_info_ptr;
        struct bst_node_ *tmp_ptr = (*current_bst)->root;
        struct bst_node_ *saveptr;
        char writebuf[PW_CHILD_READ_SIZE] = {};
        memset(writebuf, 0, PW_CHILD_READ_SIZE);
        pid_t *pid_ptr = (pid_t *)writebuf;
        *pid_ptr  = -1;
        ssize_t tmp __attribute__((unused)) = 0;

        while (NULL != tmp_ptr) {
                if (NULL != tmp_ptr->link[BST_LEFT]) {
                        saveptr = tmp_ptr->link[BST_LEFT];
                        tmp_ptr->link[BST_LEFT] = saveptr->link[BST_RIGHT];
                        saveptr->link[BST_RIGHT] = tmp_ptr;
                } else {
                        saveptr = tmp_ptr->link[BST_RIGHT];

                        switch (type) {
                        case PW_PID_BST:
                                pid_info_ptr = tmp_ptr->memblk;
                                close(pid_info_ptr->ipc_fdes[0]);
                                tmp = write(pid_info_ptr->ipc_fdes[1],
                                            writebuf,
                                            PW_CHILD_READ_SIZE);
                                close(pid_info_ptr->ipc_fdes[1]);
                                break;
                        case PW_IDLE_BST:
                                idle_info_ptr = tmp_ptr->memblk;
                                close(idle_info_ptr->ipc_fdes[0]);
                                tmp = write(idle_info_ptr->ipc_fdes[1],
                                            writebuf,
                                            PW_CHILD_READ_SIZE);
                                close(idle_info_ptr->ipc_fdes[1]);
                                break;
                        case PW_CLIENT_BST:
                                /*
                                 *the client case has already been taken care
                                 *of by pw_client_bst_report_helper()
                                 */
                                break;
                        }
                        free(tmp_ptr->memblk);
                        free(tmp_ptr);
                }
                tmp_ptr = saveptr;
        }
        free(*current_bst);
        *current_bst = NULL;
        return 0;
}
