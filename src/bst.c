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
#include "procwatch.h"
#include "pwlog.h"
#include "pwwrapper.h"

#include "memwatch.h"

enum bst_link_dir { BST_LEFT, BST_RIGHT, BST_LINKSIZE };

static pid_t delqueue[PW_LINEBUF_SIZE] = {};
static size_t delqueue_idx             = 0;
static unsigned interval               = 0;
static struct bst *idle_bst            = NULL;
static FILE *logfile                   = NULL;

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

int bst_destroy(struct bst **current_bst)
{
        if (NULL == current_bst || NULL == *current_bst) {
                errno = EINVAL;
                return -1;
        }

        struct pw_pid_info *pid_info_ptr;
        struct bst_node_ *tmp_ptr = (*current_bst)->root;
        struct bst_node_ *saveptr;
        char writebuf[PW_CHILD_READ_SIZE] = {};
        memset(writebuf, 0, PW_CHILD_READ_SIZE);
        pid_t *pid_ptr = (pid_t *)writebuf;
        *pid_ptr  = -1;
        ssize_t tmp = 0;

        while (NULL != tmp_ptr) {
                if (NULL != tmp_ptr->link[BST_LEFT]) {
                        saveptr = tmp_ptr->link[BST_LEFT];
                        tmp_ptr->link[BST_LEFT] = saveptr->link[BST_RIGHT];
                        saveptr->link[BST_RIGHT] = tmp_ptr;
                } else {
                        saveptr = tmp_ptr->link[BST_RIGHT];
                        pid_info_ptr = tmp_ptr->memblk;
                        close(pid_info_ptr->ipc_fdes[0]);
                        tmp = write(pid_info_ptr->ipc_fdes[1],
                                    writebuf,
                                    PW_CHILD_READ_SIZE);
                        close(pid_info_ptr->ipc_fdes[1]);
                        free(tmp_ptr->memblk);
                        free(tmp_ptr);
                }
                tmp_ptr = saveptr;
        }
        free(*current_bst);
        *current_bst = NULL;
        return 0;
}

static void pw_pid_bst_inorder_add(struct bst_node_ *root)
{
        if (NULL != root) {
                pw_pid_bst_inorder_add(root->link[BST_LEFT]);

                struct pw_pid_info *tmp_ptr = root->memblk;
                /*from The CERT ® C Coding Standard 2e*/
                if ((UINT_MAX - interval) < tmp_ptr->pwait_threshold) {
                        tmp_ptr->pwait_threshold = UINT_MAX;
                } else {
                        tmp_ptr->pwait_threshold += interval;
                }
                pw_pid_bst_inorder_add(root->link[BST_RIGHT]);
        }
}

int pw_pid_bst_interval_add(struct bst *current_bst, unsigned ival)
{
        if (NULL == current_bst) {
                errno = EINVAL;
                return -1;
        }
        interval = ival;
        pw_pid_bst_inorder_add(current_bst->root);

        return 0;
}

static void pw_pid_bst_refresh_helper(struct bst_node_ *root)
{
        if (NULL == root)
                return;
        pw_pid_bst_refresh_helper(root->link[BST_LEFT]);
        bool have_idle = false;
        char linebuf[16] = {};
        struct pw_pid_info *pid_info_ptr = root->memblk;
        struct pw_idle_info *idle_info_ptr;

        if (pid_info_ptr->pwait_threshold >=
            pid_info_ptr->cwait_threshold) {
                read_or_die(pid_info_ptr->ipc_fdes[0],
                            linebuf,
                            2);
                /*failed to kill the specified process*/
                if (0 == strcmp(linebuf, "0")) {
                        have_idle = true;
                /*successfully killed the process*/
                } else if (0 == strcmp(linebuf, "1")) {
                        have_idle = true;
                        pid_info_ptr->type = ACTION_KILL;
                        pwlog_write(logfile, pid_info_ptr);
                        num_killed++;
                /*invalid pipe message, child quits*/
                } else if (0 == strcmp(linebuf, "2")) {
                        close_or_die(pid_info_ptr->ipc_fdes[0]);
                        close_or_die(pid_info_ptr->ipc_fdes[1]);
                }

                if (true == have_idle) {
                        idle_info_ptr =
                                bst_add(idle_bst,
                                        pid_info_ptr->child_pid,
                                        1,
                                        sizeof(struct pw_idle_info));
                        idle_info_ptr->child_pid =
                                pid_info_ptr->child_pid;
                        idle_info_ptr->ipc_fdes[0] =
                                pid_info_ptr->ipc_fdes[0];
                        idle_info_ptr->ipc_fdes[1] =
                                pid_info_ptr->ipc_fdes[1];
                }
                delqueue[delqueue_idx] =
                        pid_info_ptr->watched_pid;
                delqueue_idx++;
        }
        pw_pid_bst_refresh_helper(root->link[BST_RIGHT]);
}

int pw_pid_bst_refresh(struct bst *pw_pid_bst,
                       struct bst *pw_idle_bst,
                       FILE *pwlog)
{
        if (NULL == pw_pid_bst || NULL == pw_idle_bst) {
                errno = EINVAL;
                return -1;
        }
        idle_bst = pw_idle_bst;
        logfile = pwlog;

        pw_pid_bst_refresh_helper(pw_pid_bst->root);

        for (size_t i = 0; i < delqueue_idx; ++i) {
                bst_del(pw_pid_bst, delqueue[i]);
        }
        delqueue_idx = 0;
        return 0;
}
