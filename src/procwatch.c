#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

#include "cfgparser.h"
#include "procwatch.h"
#include "pwlog.h"
#include "pwwrapper.h"

#define PW_ONLY
#include "pwutils.h"
#undef PW_ONLY

#include "memwatch.h"

/*
static struct pw_watched_pid_info *pid_pair_array = NULL;
static size_t pid_pair_array_size                 = 0;
static size_t pid_pair_array_index                = 0;
*/
static volatile sig_atomic_t sig_hup_flag = false;
static volatile sig_atomic_t sig_int_flag = false;

void procwatch(const char *const cfgname)
{
        /*Make stdout unbuffered.*/
        setbuf(stdout, NULL);
        procclean();

        /*
         *This array is used to store the watched pid pairs
         */

        /*
        pid_pair_array_size = 256;
        pid_pair_array = calloc_or_die(pid_pair_array_size,
                                       sizeof(struct pw_watched_pid_info));
        */
        char linebuf[PW_LINEBUF_SIZE] = {};
        FILE *pwlog = pwlog_setup();
        struct pw_log_info loginfo = {};
        int child_return_status;
        size_t numcfgline = config_parse(cfgname);

        for (size_t i = 0; i < numcfgline; ++i) {
                loginfo.process_name   = pw_cfg_vector[i].process_name;
                loginfo.wait_threshold = pw_cfg_vector[i].wait_threshold;
                work_dispatch(pwlog, &loginfo);
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

static void work_dispatch(FILE *pwlog, struct pw_log_info *const loginfo)
{
        bool process_not_found = true;
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr = NULL;
        FILE *pidof_pipe = pidof_popen(loginfo->process_name);
        uintmax_t watched_pid = 0;
        pid_t child_pid;

        while (NULL != fgets(linebuf, sizeof linebuf, pidof_pipe)) {
                process_not_found = false;
                watched_pid = strtoumax(linebuf, &endptr, 10);

                child_pid = fork_or_die();
                switch (child_pid) {
                case 0:
                        pclose(pidof_pipe);
                        fclose_or_die(pwlog);
                        process_monitor(loginfo->wait_threshold,
                                        (pid_t)watched_pid);
                        /*NEVER REACHED AGAIN*/
                        break;
                default:
                        /*Parent write INFO_INIT log*/
                        loginfo->log_type = INFO_INIT;
                        loginfo->watched_pid = (pid_t)watched_pid;
                        pwlog_write(pwlog, loginfo);
                        /*
                        pid_array_update(child_pid,
                                         watched_pid,
                                         loginfo->process_name,
                                         loginfo->wait_threshold);
                        */
                        break;
                }
        }
        if (true == process_not_found) {
                /*Parent write INFO_NOEXIST log*/
                loginfo->log_type = INFO_NOEXIST;
                pwlog_write(pwlog, loginfo);
        }
        pclose_or_die(pidof_pipe);
}

static void process_monitor(unsigned wait_threshold, pid_t watched_process_id)
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
                        exit(EXIT_FAILURE);
                        break;
                }
        }
        /*
        pid_array_destroy();
        */
        exit(EXIT_SUCCESS);
}

static FILE *pidof_popen(const char *const process_name)
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

/*
static void pid_array_update(pid_t child_pid,
                             pid_t watched_pid,
                             const char *process_name,
                             unsigned wait_threshold)
{
        if (pid_pair_array_index == pid_pair_array_size) {
                pid_pair_array_size *= 2;
                pid_pair_array = realloc_or_die(pid_pair_array,
                                                pid_pair_array_size *
                                                sizeof(struct pw_watched_pid_info));
        }
        pid_pair_array[pid_pair_array_index].child_pid = child_pid;
        pid_pair_array[pid_pair_array_index].watched_pid = watched_pid;
        pid_pair_array[pid_pair_array_index].process_name = 
                calloc_or_die(1, strlen(process_name) + 1);
        strcpy(pid_pair_array[pid_pair_array_index].process_name, process_name);
        pid_pair_array[pid_pair_array_index].wait_threshold = wait_threshold;
        pid_pair_array_index++;
}

static void pid_array_destroy(void)
{
        for (size_t i = 0; i < pid_pair_array_index; ++i) {
                zerofree(pid_pair_array[i].process_name);
        }
        zerofree(pid_pair_array);
}
*/
