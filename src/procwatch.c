#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bst.h"
#include "cfgparser.h"
#include "procwatch.h"
#include "pwlog.h"
#include "pwwrapper.h"

#define PW_ONLY
#include "pwutils.h"
#undef PW_ONLY

#include "memwatch.h"

size_t num_killed                               = 0;
/*pw_pid_bst is used for storing struct pw_pid_info*/
static struct bst *pw_pid_bst                   = NULL;
/*pw_idle_bst is used for storing the info of idle children*/
static struct bst *pw_idle_bst                  = NULL;
static FILE *pwlog                              = NULL;
static pid_t delele_ready_pids[PW_LINEBUF_SIZE] = {};
static size_t delele_ready_idx                  = 0;
static volatile sig_atomic_t sig_hup_flag       = false;
static volatile sig_atomic_t sig_int_flag       = false;

void procwatch(const char *const cfgname)
{
        /*Make stdout unbuffered.*/
        setbuf(stdout, NULL);
        procclean();

        pw_pid_bst = bst_init();
        pw_idle_bst = bst_init();
        char linebuf[PW_LINEBUF_SIZE] = {};
        pwlog = pwlog_setup();
        int child_return_status;
        size_t numcfgline = config_parse(cfgname);
        struct bst_trav trav;
        struct pw_pid_info *pid_info_ptr;
        struct pw_idle_info *idle_info_ptr;
        pid_t saved_monitored_id;

        while (true) {
                for (size_t i = 0; i < numcfgline; ++i) {
                        work_dispatch(&pw_cfg_vector[i]);
                }
                sleep(5U);
                pw_pid_bst_add_interval(pw_pid_bst, 5U);
                pid_info_ptr = bst_first(&trav, pw_pid_bst);
                while (NULL != pid_info_ptr) {
                        if (pid_info_ptr->pwait_threshold >=
                            pid_info_ptr->cwait_threshold) {
                                read_or_die(pid_info_ptr->ipc_fdes[0],
                                            linebuf,
                                            2);
                                /*failed to kill the specified process*/
                                if (0 == strcmp(linebuf, "0")) {
                                        pid_info_ptr->type = INFO_NOEXIST;
                                /*successfully killed the process*/
                                } else if (0 == strcmp(linebuf, "1")) {
                                        pid_info_ptr->type = ACTION_KILL;
                                        num_killed++;
                                /*invalid pipe message, child quits*/
                                } else if (0 == strcmp(linebuf, "2")) {
                                }

                                if (INFO_NOEXIST == pid_info_ptr->type ||
                                    ACTION_KILL == pid_info_ptr->type) {
                                        pwlog_write(pwlog, pid_info_ptr);
                                        idle_info_ptr =
                                                bst_add(pw_idle_bst,
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
                                delele_ready_pids[delele_ready_idx] =
                                        pid_info_ptr->watched_pid;
                                delele_ready_idx++;
                        }
                        pid_info_ptr = bst_next(&trav);
                }
                for (size_t i = 0; i < delele_ready_idx; ++i) {
                        bst_del(pw_pid_bst, delele_ready_pids[i]);
                }
        }

        /*
         *Once the main process has dispatched the work to all the
         *worker processes, wait for them to finish so they would
         *not become zombies.
         */
        /*
        errno = 0;
        loginfo.num_term = 0;
        for (size_t i = 0; i < pid_pair_array_index; ++i) {
                waitpid(pid_pair_array[i].child_pid, &child_return_status, 0);
                if (WIFEXITED(child_return_status) &&
                    EXIT_SUCCESS == WEXITSTATUS(child_return_status)) {
                        loginfo.num_term++;
                        loginfo.log_type = ACTION_KILL;
                        loginfo.watched_pid = pid_pair_array[i].watched_pid;
                        loginfo.process_name = pid_pair_array[i].process_name;
                        loginfo.wait_threshold = pid_pair_array[i].wait_threshold;
                        pwlog_write(pwlog, &loginfo);
                        zerofree(pid_pair_array[i].process_name);
                }
        }

        loginfo.log_type = INFO_REPORT;
        pwlog_write(pwlog, &loginfo);
        fclose_or_die(pwlog);
        zerofree(pid_pair_array);
        */
        bst_destroy(&pw_pid_bst);
        bst_destroy(&pw_idle_bst);
}

static void procclean(void)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         */
        FILE *clean_pipe              =
                popen_or_die("pidof -x procnanny | tr \' \' \'\n\'", "r");
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr                  = NULL;
        const pid_t pw_id             = getpid();
        uintmax_t zombie_id           = 0;
        errno                         = 0;

        while (NULL != fgets(linebuf, sizeof linebuf, clean_pipe)) {
                zombie_id = strtoumax(linebuf, &endptr, 10);

                if (pw_id == (pid_t)zombie_id)
                        continue;

                if (0 != kill((pid_t)zombie_id, SIGKILL)) {
                        perror("kill()");
                        exit(EXIT_FAILURE);
                }
        }
        pclose_or_die(clean_pipe);
}

static void work_dispatch(const struct pw_cfg_info *const cfginfo)
{
        bool process_not_found = true;
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr = NULL;
        FILE *pidof_pipe = pidof_popenr(cfginfo->process_name);
        struct pw_pid_info wpid_info = {
                .child_pid       = 0,
                .watched_pid     = 0,
                .cwait_threshold = cfginfo->wait_threshold,
                .pwait_threshold = 0,
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
                } else {
                        child_create(pidof_pipe, &wpid_info);
                }
                /*Parent write INFO_INIT log*/
                wpid_info.type = INFO_INIT;
                pwlog_write(pwlog, &wpid_info);
                parent_msg_write(&wpid_info);
        }
        if (true == process_not_found) {
                /*Parent write INFO_NOEXIST log*/
                wpid_info.type = INFO_NOEXIST;
                pwlog_write(pwlog, &wpid_info);
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
                |  "2"   | Invalid message size(quit)         |
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

        *((pid_t *)writebuf)                     =
                wpid_info->watched_pid;
        *((unsigned *)(((pid_t *)writebuf) + 1)) =
                wpid_info->cwait_threshold;
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
                bst_destroy(&pw_pid_bst);
                bst_destroy(&pw_idle_bst);
                pclose(pidof_pipe);
                fclose_or_die(pwlog);
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
         *For extra safety, block all (except SIGINT and SIGKILL) signals
         *in case the SIGINT or SIGHUP signal is generated on the controlling
         *terminal; which would send the signal to the whole process group.
         */
        sigset_t block_mask;
        sigfillset_or_die(&block_mask);
        sigprocmask_or_die(SIG_SETMASK, &block_mask, NULL);
        const char *READ_PIPE_FAIL_MSG       = "child: invalid message size\n";
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
                        write_or_die(STDERR_FILENO,
                                     READ_PIPE_FAIL_MSG,
                                     strlen(READ_PIPE_FAIL_MSG));
                        write_or_die(writedes, "2", 2);
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
