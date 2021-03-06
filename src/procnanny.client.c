#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h> /*definitoin of getaddrinfo()*/
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bst.h"
#include "bstclient.h"
/*
 *Note even though the cfgparser header is used, the client code
 *does not link to cfgparser object file because client is NEVER
 *expected to parse any configuration files by itself.
 */
#include "cfgparser.h"
#include "procclient.h"
#include "pwversion.h"
#include "pwwrapper.h"

#define PWCLIENT_ONLY
#include "pwcutils.h"
#undef PWCLIENT_ONLY

#include "memwatch.h"

static bool cfg_resent         = true;
static int server_sockfd       = 0;
/*pw_pid_bst is used for storing struct pw_pid_info*/
static struct bst *pw_pid_bst  = NULL;
/*pw_idle_bst is used for storing the info of idle children*/
static struct bst *pw_idle_bst = NULL;
/*client_cfg_vector is used as a local storage for the received configuration*/
static struct pw_cfg_info client_cfg_vector[PW_CFG_MAX_NUM_PROGRAM_NAME];

int main(int argc, char *argv[])
{
        if (3 != argc || strlen(argv[2]) != strspn(argv[2], "0123456789")) {
                errx(EXIT_FAILURE,
                     "Usage: %s [HOST NAME] [PORT NUMBER]",
                     argv[0]);
        }
        /*
         *For extra safety, block all (except SIGSTOP and SIGKILL) signals
         *in case the SIGINT or SIGHUP signal is generated on the controlling
         *terminal; which would send the signal to the whole process group.
         *Note in Part III the clients do not expect to respond to any signals:
         *all the children processes spawned would inherit the signal mask,
         *which is acceptable.
         */
        sigset_t block_mask;

        sigfillset_or_die(&block_mask);
        sigprocmask_or_die(SIG_SETMASK, &block_mask, NULL);
        /*Make stdout unbuffered.*/
        setbuf(stdout, NULL);
        procclean((enum pw_clean_type)PW_CLIENT_CLEAN);

        struct addrinfo server_info = {
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
                .ai_flags = 0,
                .ai_protocol = 0
        };
        struct addrinfo *host_list      = NULL;
        struct addrinfo *host_list_iter = NULL;
        int gai_return_val = getaddrinfo(argv[1], argv[2],
                                         &server_info, &host_list);
        if (0 != gai_return_val) {
                errx(EXIT_FAILURE,
                     "getaddrinfo(): %s",
                     gai_strerror(gai_return_val));
        }

        for (host_list_iter = host_list;
             NULL != host_list_iter;
             host_list_iter = host_list_iter->ai_next) {
                server_sockfd = socket(host_list_iter->ai_family,
                                       host_list_iter->ai_socktype,
                                       host_list_iter->ai_protocol);
                if (-1 != connect(server_sockfd,
                                  host_list_iter->ai_addr,
                                  host_list_iter->ai_addrlen)) {
                        break;
                }
                close_or_die(server_sockfd);
        }

        if (NULL == host_list_iter) {
                errx(EXIT_FAILURE,
                     "Host %s with Port Number %s is not reachable",
                     argv[1],
                     argv[2]);
        }
        freeaddrinfo(host_list);
        host_list = NULL;
        procwatch();
        close_or_die(server_sockfd);
        return EXIT_SUCCESS;
}

static void procwatch(void)
{
        /*
         *Initialize the file descriptor set to contain the
         *socket descriptor of server
         */
        fd_set chkset;
        fd_set tmpset;
        FD_ZERO(&chkset);
        FD_SET(server_sockfd, &chkset);
        struct timespec ts = {
                .tv_sec  = 5,
                .tv_nsec = 0
        };
        sigset_t select_mask;
        sigfillset_or_die(&select_mask);
        struct pw_cfg_info *cfg_iterator = NULL;
        int numdes;
        size_t numcfgline;
        pw_pid_bst = bst_init();
        pw_idle_bst = bst_init();

        if (0 != atexit(clean_up)) {
                bst_destroy(&pw_pid_bst, (enum bst_type)PW_PID_BST);
                bst_destroy(&pw_idle_bst, (enum bst_type)PW_IDLE_BST);
                errx(EXIT_FAILURE, "atexit() : failed to register clean_up()");
        }

        /*
         *For the 1st time the client read the configuration file,
         *it is possible the data_read would cause the client to
         *be blocked (network delay, etc), this case is IGNORED.
         */
        data_read(server_sockfd, client_cfg_vector, sizeof client_cfg_vector);

        while (true) {
                for (cfg_iterator = client_cfg_vector, numcfgline = 0;
                     '\0' != cfg_iterator->process_name[0] &&
                     0 != cfg_iterator->wait_threshold;
                     ++cfg_iterator, ++numcfgline) {
                        work_dispatch(cfg_iterator, &chkset);
                }
                cfg_resent = false;
                /*
                 *A SIGINT signal is received at server's side.
                 */
                if (0 == numcfgline) {
                        break;
                }
                tmpset = chkset;
                numdes = pselect(getdtablesize(), &tmpset,
                                 NULL, NULL, &ts, &select_mask);
                switch (numdes) {
                /*
                 *Since the pselect() cannot be interrupted by a signal
                 *if the return value is -1, there must be some error occur.
                 */
                case -1:
                        perror("pselect()");
                        exit(EXIT_FAILURE);
                /*
                 *A 5 second timeout occurred and no file descriptors are
                 *available yet.
                 */
                case 0:
                        break;
                default:
                        /*A SIGHUP/SIGINT signal is received at server's side.*/
                        if (FD_ISSET(server_sockfd, &tmpset)) {
                                data_read(server_sockfd,
                                          client_cfg_vector,
                                          sizeof client_cfg_vector);
                                cfg_resent = true;
                                FD_CLR(server_sockfd, &tmpset);
                        }
                        pw_pid_bst_refresh(pw_pid_bst,
                                           pw_idle_bst,
                                           &tmpset,
                                           server_sockfd);
                }
        }
}

static void work_dispatch(const struct pw_cfg_info *cfginfo, fd_set *chksetptr)
{
        bool process_not_found = true;
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr = NULL;
        FILE *pidof_pipe = pidof_popenr(cfginfo->process_name);
        struct pw_pid_info wpid_info = {
                .child_pid       = 0,
                .watched_pid     = 0,
                .wait_threshold = cfginfo->wait_threshold,
        };
        strcpy(wpid_info.process_name, cfginfo->process_name);
        struct pw_idle_info *idle_info_ptr;

        while (NULL != fgets(linebuf, sizeof linebuf, pidof_pipe)) {
                process_not_found = false;
                wpid_info.watched_pid = (pid_t)strtoumax(linebuf, &endptr, 10);
                /*
                 *not possible for wpid_bst to be NULL here
                 *so errno is un-checked;
                 *if the pid is already monitored, proceed
                 *to the next pid
                 */
                if (NULL != bst_find(pw_pid_bst, wpid_info.watched_pid))
                        continue;
                /*use exsiting children if there are idle ones*/
                if (false == bst_isempty(pw_idle_bst)) {
                        idle_info_ptr =
                                bst_find(pw_idle_bst, bst_rootkey(pw_idle_bst));
                        wpid_info.child_pid = idle_info_ptr->child_pid;
                        wpid_info.ipc_fdes[0] = idle_info_ptr->ipc_fdes[0];
                        wpid_info.ipc_fdes[1] = idle_info_ptr->ipc_fdes[1];
                        bst_del(pw_idle_bst, bst_rootkey(pw_idle_bst));
                /*otherwise there is no idle children, create new processes*/
                } else {
                        /*
                         *the read end(pipe1) of the child process needed to be
                         *added to the chkset to be used in the future pselect()
                         *calls; in other words, the pwait_threshold is no
                         *longer needed in part III so that field is removed
                         *from the struct pw_pid_info
                         */
                        child_create(pidof_pipe, &wpid_info);
                        FD_SET(wpid_info.ipc_fdes[0], chksetptr);
                }
                /*Parent pass the INFO_INIT log to server*/
                wpid_info.type = INFO_INIT;
                data_write(server_sockfd, &wpid_info, sizeof wpid_info);
                /*pwlog_write(pwlog, &wpid_info);*/
                parent_msg_write(&wpid_info);
        }
        if (true == process_not_found && true == cfg_resent) {
                /*
                 *Parent passes the INFO_NOEXIST log to server and make sure
                 *that the INFO_NOEXIST only passes back upon configuration
                 *resend (a.k.a SIGHUP received at server side)
                 */
                wpid_info.type = INFO_NOEXIST;
                data_write(server_sockfd, &wpid_info, sizeof wpid_info);
                /*pwlog_write(pwlog, &wpid_info);*/
        }
        pclose_or_die(pidof_pipe);
}

              /*Communication Protocol between parent and child*/
/*
                           +------+--------+-------+
                           |      | parent | child |
                           +------+--------+-------+
                           |pipe1 | read   | write |
                           |pipe2 | write  | read  |
                           +------+--------+-------+

                              from Child Processes
                +--------+------------------------------------+
                |Content | Meaning                            |
                +--------+------------------------------------+
                |  "0"   | Failed to kill the specied process |
                |  "1"   | Success                            |
                |  "2"   | Invalid message size(quit debug)   |
                +--------+------------------------------------+

                              from Parent Process
                           +------------------------+
                           |pid_t    watched_pid    |
                           |unsigned wait_threshold |
                           +------------------------+
 */
static void parent_msg_write(struct pw_pid_info *const wpid_info)
{
        char writebuf[PW_CHILD_READ_SIZE] = {};
        struct pw_pid_info *wpid_info_ptr;
        pid_t *pid_ptr = (pid_t *)writebuf;
        unsigned *unsigned_ptr = (unsigned *)(pid_ptr + 1);

        *pid_ptr = wpid_info->watched_pid;
        *unsigned_ptr = wpid_info->wait_threshold;
        write_or_die(wpid_info->ipc_fdes[1],
                     writebuf,
                     PW_CHILD_READ_SIZE);

        errno = 0;
        wpid_info_ptr = bst_add(pw_pid_bst,
                          wpid_info->watched_pid,
                          1,
                          sizeof(struct pw_pid_info));
        /*
         *note that duplicate keys are not possible because
         *it is checked previously; so the only case the
         *bst_add() can fail is SIZE_MAX is about to be reached
         *(impossible for wpid_bst to be NULL)
         */
        if (NULL != wpid_info_ptr) {
                *wpid_info_ptr = *wpid_info;
        } else if (EOVERFLOW == errno || EINVAL == errno) {
                        errx(EXIT_FAILURE, "bst : error");
        }
        /*
         *the watched_pid has already been monitored, do nothing
         */
}

/*
 *This function creates another child aned setups the ipc channel between the
 *newly created child and the caller.
 *The ipc channel is recorded in the ipc_fdes field of struct pw_pid_info.
 *The child calls process_monitor() and NEVER returns.
 */
static void child_create(FILE *pidof_pipe, struct pw_pid_info *const wpid_info)
{
        /*
         *note I use 2 parallel unidirectional pipes to do ipc
         *between child and parent
         */
        int tmp_fdes1[2], tmp_fdes2[2];
        pipe_or_die(tmp_fdes1);
        pipe_or_die(tmp_fdes2);
        wpid_info->child_pid = fork_or_die();

        switch (wpid_info->child_pid) {
        case 0:
                pclose(pidof_pipe);
                /*fclose_or_die(pwlog);*/
                close_or_die(tmp_fdes1[0]);
                close_or_die(tmp_fdes2[1]);
                process_monitor(tmp_fdes2[0], tmp_fdes1[1]);
                /*NEVER REACHED AGAIN*/
        default:
                close_or_die(tmp_fdes1[1]);
                close_or_die(tmp_fdes2[0]);
                wpid_info->ipc_fdes[0] = tmp_fdes1[0];
                wpid_info->ipc_fdes[1] = tmp_fdes2[1];
                return;
        }
}

static void process_monitor(int readdes, int writedes)
{
        /*
        const char *READ_PIPE_FAIL_MSG       = "child: invalid message size\n";
        */
        char        readbuf[PW_CHILD_READ_SIZE] = {};
        pid_t       watched_pid              = 0;
        pid_t      *watched_pid_ptr          = NULL;
        unsigned    wait_threshold           = 0;
        unsigned   *wait_threshold_ptr       = NULL;
        memset(readbuf, 0, PW_CHILD_READ_SIZE);
        /*
         *Even though the checking is performed on errno after the
         *process is killed, we still have a potential problem
         *of killing some other processes end up with the
         *pid same as the one before;
         *The recycling nature of pid is IGNORED.
         */
        while (true) {
                /*
                 *the read would block the child once it is waiting for
                 *the parent to give information about the next process
                 *to monitor
                 */
                if (PW_CHILD_READ_SIZE !=
                    read(readdes, readbuf, PW_CHILD_READ_SIZE)){
                        /*
                         *Posix 1 guarantees that a read of size less than
                         *PIPE_BUF is atomic, here since CHILD_READ_SIZE is
                         *less than PIPE_BUF(512 bytes), so normally this read()
                         *call would succeed
                         */

                        /*
                        write_or_die(STDERR_FILENO,
                                     READ_PIPE_FAIL_MSG,
                                     strlen(READ_PIPE_FAIL_MSG));
                        write_or_die(writedes, "2", 2);
                        */
                        break;
                }

                watched_pid_ptr = (pid_t *)readbuf;
                watched_pid = *watched_pid_ptr;
                wait_threshold_ptr = (unsigned *)(watched_pid_ptr + 1);
                wait_threshold = *wait_threshold_ptr;

                if (-1 == watched_pid)
                        break;
                sleep(wait_threshold);
                /*
                 *This is a guaranteed kill if the listed 2 cases
                 *do not apply, so SIGKILL is used rather than SIGTERM
                 */
                errno = 0;
                if (0 != kill(watched_pid, SIGKILL)) {
                        switch (errno) {
                        case EPERM:
                                /*
                                 *This process does not have the permission
                                 *to kill the watched process.
                                 */
                        case ESRCH:
                                /*
                                 *If the watched process no longer exist,
                                 *the monitor action is considered "failed".
                                 */
                        default: /*Added for extra safety*/ 
                                write_or_die(writedes, "0", 2);
                        }
                } else {
                        write_or_die(writedes, "1", 2);
                }
        }
        close_or_die(readdes);
        close_or_die(writedes);
        exit(EXIT_SUCCESS);
}

static FILE *pidof_popenr(const char *const process_name)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         */
        const char *const pidof_filter = "pidof -x ";
        const char *const tr_filter = " | tr \' \' \'\n\'";
        FILE *pidof_pipe = NULL;
        size_t cat_str_size =
                strlen(pidof_filter) +
                strlen(process_name) +
                strlen(tr_filter)    + 1;
        char *cmd_buffer = calloc_or_die(1, cat_str_size);

        snprintf(cmd_buffer, cat_str_size, "%s%s%s",
                 pidof_filter,
                 process_name,
                 tr_filter);
        pidof_pipe = popen_or_die(cmd_buffer, "r");
        zerofree(cmd_buffer);

        return pidof_pipe;
}

static void clean_up(void)
{
        bst_destroy(&pw_pid_bst, (enum bst_type)PW_PID_BST);
        bst_destroy(&pw_idle_bst, (enum bst_type)PW_IDLE_BST);
}
