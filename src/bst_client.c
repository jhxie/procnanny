#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>

#include "bst.h"
#include "bstclient.h"
#include "procclient.h"
#include "pwwrapper.h"

#include "memwatch.h"

static pid_t delqueue[PW_LINEBUF_SIZE] = { 0 };
static size_t delqueue_idx             = 0;
static struct bst *idle_bst            = NULL;
static const fd_set *childset          = 0;
static int serverfd                    = 0;

static void pw_pid_bst_refresh_helper(struct bst_node_ *root)
{
        if (NULL == root)
                return;
        pw_pid_bst_refresh_helper(root->link[BST_LEFT]);
        bool have_idle = false;
        char linebuf[16] = {};
        struct pw_pid_info *pid_info_ptr = root->memblk;
        struct pw_idle_info *idle_info_ptr;

        /*if (pid_info_ptr->pwait_threshold >=*/
            /*pid_info_ptr->cwait_threshold) {*/
        if (FD_ISSET(pid_info_ptr->ipc_fdes[0], childset)) {
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
                        /*pwlog_write(logfile, pid_info_ptr, NULL);*/
                        data_write(serverfd,
                                   pid_info_ptr,
                                   sizeof(struct pw_pid_info));
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
                       const fd_set *tmpset,
                       const int server_sockfd)
{
        if (NULL == pw_pid_bst || NULL == pw_idle_bst) {
                errno = EINVAL;
                return -1;
        }
        idle_bst = pw_idle_bst;
        childset = tmpset;
        serverfd = server_sockfd;

        pw_pid_bst_refresh_helper(pw_pid_bst->root);

        for (size_t i = 0; i < delqueue_idx; ++i) {
                bst_del(pw_pid_bst, delqueue[i]);
        }
        delqueue_idx = 0;
        return 0;
}
