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

/*
 *watched_pid_bst is used for storing struct pw_watched_pid_info
 */
static struct bst *wpid_bst = NULL;
static volatile sig_atomic_t sig_hup_flag = false;
static volatile sig_atomic_t sig_int_flag = false;

void procwatch(const char *const cfgname)
{
        /*Make stdout unbuffered.*/
        setbuf(stdout, NULL);
        procclean();

        wpid_bst = bst_init();
        char linebuf[PW_LINEBUF_SIZE] = {};
        FILE *pwlog = pwlog_setup();
        struct pw_log_info loginfo = {};
        int child_return_status;
        size_t numcfgline = config_parse(cfgname);

        for (size_t i = 0; i < numcfgline; ++i) {
                work_dispatch(pwlog, &pw_cfg_vector[i]);
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
        bst_destroy(&wpid_bst);
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

static void work_dispatch(FILE *pwlog, const struct pw_cfg_info *const cfginfo)
{
        bool process_not_found = true;
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr = NULL;
        FILE *pidof_pipe = pidof_popenr(cfginfo->process_name);
        struct pw_log_info loginfo = {
                .wait_threshold = cfginfo->wait_threshold,
                .process_name = cfginfo->process_name
        };
        struct pw_watched_pid_info wpid_info = {
                .child_pid = 0,
                .watched_pid = 0,
                .cwait_threshold = cfginfo->wait_threshold,
                .pwait_threshold = 0,
        };
        struct pw_watched_pid_info *tmp_ptr;
        strcpy(wpid_info.process_name, cfginfo->process_name);
        uintmax_t watched_pid = 0;
        pid_t child_pid;
        int tmp_fdes1[2], tmp_fdes2[2];

        while (NULL != fgets(linebuf, sizeof linebuf, pidof_pipe)) {
                process_not_found = false;
                watched_pid = strtoumax(linebuf, &endptr, 10);

                /*
                 *note I use 2 parallel unidirectional pipes to do ipc
                 *between child and parent
                 */

                /*fill in the rest wpid_info*/
                /*
                 * +------+--------+-------+
                 * |      | parent | child |
                 * +------+--------+-------+
                 * |pipe1 | read   | write |
                 * |pipe2 | write  | read  |
                 * +------+--------+-------+
                 */
                pipe_or_die(tmp_fdes1);
                pipe_or_die(tmp_fdes2);
                /*fill in the rest wpid_info*/

                child_pid = fork_or_die();
                switch (child_pid) {
                case 0:
                        WORK_DISPATCH_CHILD_CLEANUP();
                        process_monitor((pid_t)watched_pid,
                                        loginfo.wait_threshold,
                                        tmp_fdes2[0],
                                        tmp_fdes1[1]);
                        /*NEVER REACHED AGAIN*/
                        break;
                default:
                        /*Parent write INFO_INIT log*/
                        loginfo.log_type = INFO_INIT;
                        loginfo.watched_pid = (pid_t)watched_pid;
                        pwlog_write(pwlog, &loginfo);

                        WORK_DISPATCH_PARENT_CLEANUP();
                        errno = 0;
                        tmp_ptr = bst_add(wpid_bst,
                                          watched_pid,
                                          1,
                                          sizeof(struct pw_watched_pid_info));
                        if (NULL != tmp_ptr) {
                                *tmp_ptr = wpid_info;
                        } else if (EOVERFLOW == errno || EINVAL == errno) {
                                        errx(EXIT_FAILURE, "bst : error");
                        }
                        /*
                         *the watched_pid has already been monitored, do nothing
                         */
                        break;
                }
        }
        if (true == process_not_found) {
                /*Parent write INFO_NOEXIST log*/
                loginfo.log_type = INFO_NOEXIST;
                pwlog_write(pwlog, &loginfo);
        }
        pclose_or_die(pidof_pipe);
}

static void process_monitor(pid_t    watched_process_id,
                            unsigned wait_threshold,
                            int      readdes,
                            int      writedes)
{

        sleep(wait_threshold);
        /*
         *Even though the checking is performed on errno after the
         *process is killed, we still have a potential problem
         *of killing some other processes end up with the
         *pid same as the one before;
         *The recycling nature of pid is IGNORED.
         */
        errno = 0;
        /*
         *This is a guaranteed kill if the listed 2 cases
         *do not apply, so SIGKILL is used rather than SIGTERM
         */
        if (0 != kill(watched_process_id, SIGKILL)) {
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

                        /*
                        pid_array_destroy();
                        */
                        close_or_die(readdes);
                        close_or_die(writedes);
                        exit(EXIT_FAILURE);
                        break;
                }
        }
        /*
        pid_array_destroy();
        */
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
