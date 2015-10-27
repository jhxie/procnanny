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
#include <time.h>
#include <unistd.h>

#include "procwatch.h"
#include "pwwrapper.h"

#define PW_ONLY
#include "pwutils.h"
#undef PW_ONLY

#include "memwatch.h"


enum { PW_LINEBUF_SIZE = 1024 };
static struct pw_watched_pid_info *pid_pair_array = NULL;
size_t pid_pair_array_size = 0;
size_t pid_pair_array_index = 0;

void procwatch(int argc, char **argv)
{
        /*Make stdout unbuffered.*/
        setbuf(stdout, NULL);

        /*
         *This array is used to store the watched pid pairs
         */
        pid_pair_array_size = 256;
        pid_pair_array = calloc_or_die(pid_pair_array_size,
                                        sizeof(struct pw_watched_pid_info));
        procclean();
        FILE *pwlog = pwlog_setup();
        struct pw_config_info confinfo = {};
        struct pw_log_info loginfo = {};
        int child_return_status;

        while (true) {
                confinfo = config_parse(argc, argv);
                switch (confinfo.type) {
                case PW_PROCESS_NAME:
                        loginfo.process_name = confinfo.data.process_name;
                        work_dispatch(pwlog, &loginfo);
                        break;
                case PW_WAIT_THRESHOLD:
                        loginfo.wait_threshold = confinfo.data.wait_threshold;
                        break;
                case PW_END_FILE:
                        goto procwatch_loop_exit;
                        break;
                }
        }
procwatch_loop_exit:
        
        /*
         *Once the main process has dispatched the work to all the
         *worker processes, wait for them to finish so they would
         *not become zombies.
         */
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
                        pwlog_write(pwlog, &loginfo);
                        free(pid_pair_array[i].process_name);
                }
        }

        loginfo.log_type = INFO_REPORT;
        pwlog_write(pwlog, &loginfo);
        fclose_or_die(pwlog);
        free(pid_pair_array);
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

/*
 *According to the assignment specification,
 *"The program procnanny takes exactly one command-line argument that specifies
 *the full pathname to a configuration file."
 *This function checks the validty of argument vectors and returns a
 *pw_config_info structure that contains info of the config file on subsequent
 *calls on success; otherwise quit the whole program.
 *
 *Note: this function dispatches its 2 kinds of work(threashold or process name)
 *to 2 separate functions.
 */
static struct pw_config_info config_parse(int argc, char **argv)
{
        static bool parsing_threshold = true;
        static struct pw_config_info info;

        if (false == parsing_threshold) {
                info.type = PW_PROCESS_NAME;

                while (NULL !=
                       (info.data.process_name = config_parse_pname(argv[1])))
                        return info;

                /*if NULL is returned for the previous loop, we have a EOF*/
                info.type = PW_END_FILE;
                return info;
        } else {
                parsing_threshold = false;
                info.type = PW_WAIT_THRESHOLD;
        }

        if (2 != argc) {
                eprintf("Usage: %s [FILE]\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        FILE *nanny_cfg = NULL;
        nanny_cfg = fopen_or_die(argv[1], "r");
        info.data.wait_threshold = config_parse_threshold(nanny_cfg);
        fclose_or_die(nanny_cfg);
        return info;
}

/*
 *Parses 1st line of input configuration file and checks its validity:
 *returns an unsigned on success; otherwise quit the program.
 */
static unsigned config_parse_threshold(FILE *const nanny_cfg)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         *the reason for using a statically allocated array rather than
         *something like getline() is that memwatch cannot detect the memory
         *allocated by getline()
         */
        char linebuf[PW_LINEBUF_SIZE] = {};
         /*
          *dummy variable, since the case is ignored
          *where the first line is not a valid number
          */
        char *endptr = NULL;
        unsigned wait_period = 0;

        if (NULL != fgets(linebuf, sizeof linebuf, nanny_cfg)) {
                /*From Assignment #1 Description:
                 *"The first line is an integer number indicating how
                 * many seconds procnanny will run for."
                 */
                errno = 0;
                uintmax_t tmp = strtoumax(linebuf, &endptr, 10);
                if (0 == tmp && linebuf == endptr) {
                        eprintf("strtoumax() : "
                                "the first line is not a valid number\n");
                        fclose_or_die(nanny_cfg);
                        exit(EXIT_FAILURE);
                }
                /*checks overflow error*/
                if (UINTMAX_MAX == tmp && ERANGE == errno) {
                        perror("strtoumax()");
                        fclose_or_die(nanny_cfg);
                        exit(EXIT_FAILURE);
                }

                wait_period = tmp;
                if (wait_period != tmp) {
                        eprintf("strtoumax() : "
                                "number of seconds to wait "
                                "exceeds the capacity of an "
                                "unsigned integer\n");
                        fclose_or_die(nanny_cfg);
                        exit(EXIT_FAILURE);
                }
        } else {
                eprintf("The first line does not contain a valid number\n");
                fclose_or_die(nanny_cfg);
                exit(EXIT_FAILURE);
        }
        return wait_period;
}

/*
 *This function receives a file name of the configuraiton file and returns
 *a pointer to each line of the file on each subsequent calls until either
 *EOF is reached or an error occurred.
 *
 *Return Value: NULL upon EOF; char * upon an actual line
 *
 *Note: this function will _not_ close the FILE *nanny_cfg on error
 */
static char *config_parse_pname(const char *const nanny_cfg_name)
{
        static char   line_buf[PW_LINEBUF_SIZE];
        /*
         *this variable indicates the state of this function:
         *whether we have already opened the sed_filter_file or not.
         */
        static bool   in_file                 = false;
        static char   *tmp_fname              = "sed_filter_tmp";
        static fpos_t pos;
        FILE          *sed_filter_file        = NULL;

        if (true == in_file) {
                goto parse_continue;
        }

        const char *cmd_options[4]     =
        {"sed -n \'2,$p;$q\' ", nanny_cfg_name, " | uniq > ", tmp_fname};
        /*initialize the size to add an extra terminating character*/
        size_t cat_str_size = 1;

        for (size_t i = 0; i < 4; ++i)
                cat_str_size += strlen(cmd_options[i]);

        char *cmd_buffer  = calloc_or_die(1, cat_str_size);

        /*put all the strings in the buffer*/
        for (size_t i = 0; i < 4; ++i)
                strcat(cmd_buffer, cmd_options[i]);

        if (-1 == system(cmd_buffer)) {
                eprintf("system(): Error occurred "
                        "while executing child process");
                exit(EXIT_FAILURE);
        }

        free(cmd_buffer);
        cmd_buffer = NULL;

parse_continue:
        sed_filter_file = fopen_or_die(tmp_fname, "r");
        /*if the file has been opened before, recover its last position*/
        if (true == in_file) {
                fsetpos_or_die(sed_filter_file, &pos);
        } else {
                /*
                 *flip the switch if it is the 1st time opened the
                 *sed_filter_file
                 */
                in_file = true;
        }
                
        if (NULL == fgets(line_buf, sizeof line_buf, sed_filter_file)) {
                if (feof(sed_filter_file)) {
                        fclose_or_die(sed_filter_file);
                        remove(tmp_fname);
                        return NULL;
                } else {
                        perror("fgets()");
                        exit(EXIT_FAILURE);
                }
        }

        fgetpos_or_die(sed_filter_file, &pos);
        fclose_or_die(sed_filter_file);
        /*fgets() leaves the newline character untouched, so kill it*/
        line_buf[strlen(line_buf) - 1] = '\0';
        return line_buf;
}

static void work_dispatch(FILE *pwlog, struct pw_log_info *const loginfo)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         *(same as config_parse_threshold())
         */
        const char *const pidof_filter = "pidof -x ";
        const char *const tr_filter = " | tr \' \' \'\n\'";
        size_t cat_str_size =
                strlen(pidof_filter) +
                strlen(loginfo->process_name) +
                strlen(tr_filter)    + 1;
        char *cmd_buffer = calloc_or_die(1, cat_str_size);
        FILE *pidof_pipe = NULL;

        snprintf(cmd_buffer, cat_str_size, "%s%s%s",
                 pidof_filter,
                 loginfo->process_name,
                 tr_filter);

        pidof_pipe = popen_or_die(cmd_buffer, "r");

        free(cmd_buffer);
        cmd_buffer = NULL;

        bool process_not_found = true;
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr = NULL;
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
                        pid_array_update(child_pid,
                                         watched_pid,
                                         loginfo->process_name);
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
			pid_array_destroy();
                        exit(EXIT_FAILURE);
                        break;
                }
        }
        pid_array_destroy();
        exit(EXIT_SUCCESS);
}

static FILE *pwlog_setup(void)
{
        const char *const pwlog_path =secure_getenv_or_die("PROCNANNYLOGS");

        return fopen_or_die(pwlog_path, "w");
}

static void pwlog_write(FILE *pwlog, struct pw_log_info *loginfo)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         */
        char timebuf[PW_LINEBUF_SIZE] = {};
        time_t epoch_time = time(NULL);
        struct tm *cal = localtime(&epoch_time);
        strftime(timebuf, PW_LINEBUF_SIZE, "%a %b %d %T %Z %Y", cal);

        /*Note the constant reserves space for both square brackets*/
        char *timestr = calloc_or_die(1, strlen(timebuf) + 3);
        snprintf(timestr, strlen(timebuf) + 3, "[%s]", timebuf);
        /*Print the time field only*/
        fputs_or_die(timestr, pwlog);
        free(timestr);

        /*
         *For the pid_t data type, POSIX standard                    
         *guarantees that it will fit in a long integer,so we cast it 
         */
        switch (loginfo->log_type) {
        case INFO_INIT:
                fprintf(pwlog,
                        " Info: Initializing monitoring of "
                        "process \'%s\' (PID %ld).\n",
                        loginfo->process_name, (long)loginfo->watched_pid);
                break;
        case INFO_NOEXIST:
                fprintf(pwlog, " Info: No \'%s\' processes found.\n",
                        loginfo->process_name);
                break;
        case INFO_REPORT:
                fprintf(pwlog, " Info: Exiting. %zu process(es) killed.\n",
                        loginfo->num_term);
                break;
        case ACTION_KILL:
                fprintf(pwlog,
                        " Action: PID %ld (%s) killed "
                        "after exceeding %u seconds.\n",
                        (long)loginfo->watched_pid,
                        loginfo->process_name,
                        loginfo->wait_threshold);
                break;
        }
        fflush(pwlog);
}

static void pid_array_update(pid_t child_pid,
                             pid_t watched_pid,
                             const char *process_name)
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
        pid_pair_array_index++;
}

static void pid_array_destroy(void)
{
        for (size_t i = 0; i < pid_pair_array_index; ++i)
                free(pid_pair_array[i].process_name);
        free(pid_pair_array);
}
