#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>

#include "bst.h"

#include "cfgparser.h"
#include "procserver.h"
#include "pwlog.h"
#include "pwwrapper.h"

#include "memwatch.h"

static const fd_set *clientset         = NULL;
static FILE *logfile                   = NULL;

static void pw_client_bst_batchlog_helper(struct bst_node_ *root)
{
        if (NULL == root)
                return;
        pw_client_bst_batchlog_helper(root->link[BST_LEFT]);

        if (FD_ISSET(((int)root->key), clientset)) {
                struct pw_pid_info pid_info = { 0 };
                struct pw_client_info *client_info_ptr = root->memblk;
                data_read((int)root->key, &pid_info, sizeof pid_info);
                pwlog_write(logfile, &pid_info, client_info_ptr->hostname);
                if (ACTION_KILL == pid_info.type) {
                        client_info_ptr->killcnt++;
                        num_killed++;
                }
        }

        pw_client_bst_batchlog_helper(root->link[BST_RIGHT]);
}

int pw_client_bst_batchlog(struct bst *pw_client_bst,
                           const fd_set *pw_clientset,
                           FILE *pwlog)
{
        if (NULL == pw_client_bst) {
                errno = EINVAL;
                return -1;
        }
        clientset = pw_clientset;
        logfile = pwlog;

        pw_client_bst_batchlog_helper(pw_client_bst->root);
        return 0;
}

static void pw_client_bst_report_helper(struct bst_node_ *root)
{
        if (NULL == root)
                return;
        pw_client_bst_report_helper(root->link[BST_LEFT]);
        struct pw_client_info *client_info_ptr = root->memblk;
        /*Print to both stdout and logfile*/
        fprintf(logfile, "%s ", client_info_ptr->hostname);
        fprintf(stdout, "%s ", client_info_ptr->hostname);
        /*Notify the clients to quit*/
        data_write((int)root->key, pw_cfg_vector, sizeof pw_cfg_vector);
        /*Close the connection*/
        close((int)root->key);
        pw_client_bst_report_helper(root->link[BST_RIGHT]);
}

/*
 *Print the final report of all nodes.
 *Note only the HOSTNAMES of clients are printed in the helper function
 *because all the hostnames of clients are recorded in a structure within
 *a tree, each recursion only prints the hostname of the current node.
 */
int pw_client_bst_report(struct bst *pw_client_bst, FILE *pwlog)
{
        if (NULL == pw_client_bst) {
                errno = EINVAL;
                return -1;
        }
        logfile = pwlog;
        pwlog_write(logfile, &((struct pw_pid_info){
                               .type = INFO_REPORT }), NULL);
        /*
         *Set the first structure within pw_cfg_vector to a special value
         *to be sent to all the clients
         */
        memset(pw_cfg_vector, 0, sizeof pw_cfg_vector);
        pw_client_bst_report_helper(pw_client_bst->root);
        fprintf(logfile, "\n");
        fprintf(stdout, "\n");
        fflush(stdout);
        fflush(pwlog);
        return 0;
}

static void pw_client_bst_batchsend_helper(struct bst_node_ *root)
{
        if (NULL == root)
                return;

        pw_client_bst_batchsend_helper(root->link[BST_LEFT]);
        data_write((int)root->key, pw_cfg_vector, sizeof pw_cfg_vector);
        pw_client_bst_batchsend_helper(root->link[BST_RIGHT]);
}

int pw_client_bst_batchsend(struct bst *pw_client_bst, FILE *pwlog)
{
        if (NULL == pw_client_bst) {
                errno = EINVAL;
                return -1;
        }
        logfile  = pwlog;
        /*
         *Print the SIGHUP info and then resend the pw_cfg_vector
         *for each client.
         */
        pwlog_write(logfile, &((struct pw_pid_info){
                               .type = INFO_REREAD }), NULL);
        pw_client_bst_batchsend_helper(pw_client_bst->root);
        return 0;
}
