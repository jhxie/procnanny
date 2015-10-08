#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "castack.h"
#include "procwatch.h"
#include "pwwrapper.h"

#define PW_ONLY
#include "pwutils.h"
#undef PW_ONLY

#include "memwatch.h"


enum { PW_LINEBUF_SIZE = 1024 };
static struct castack *memstack = NULL;
static int pwlogfd = 0;

void procwatch(int argc, char **argv)
{
        /*Make stdout unbuffered.*/
        setbuf(stdout, NULL);
        memstack = castack_init();
        log_setup();
        PW_MEMSTACK_ENABLE_AUTO_CLEAN();

        struct pw_config_info info;
        unsigned wait_threshold = 0;
        pid_t child_pid;

        while (true) {
                info = config_parse(argc, argv);
                switch (info.type) {
                case PW_PROCESS_NAME:
                        printf("%s\n", info.data.process_name);
                        work_dispatch(wait_threshold, info.data.process_name);
                        break;
                case PW_WAIT_THRESHOLD:
                        printf("%d\n", info.data.wait_threshold);
                        wait_threshold = info.data.wait_threshold;
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
        while (true) {
                child_pid = wait(NULL);

                if (-1 == child_pid) {
                        if (ECHILD == errno) {
                                return;
                        } else {
                                perror("wait()");
                                exit(EXIT_FAILURE);
                        }
                }
        }
        close_or_die(pwlogfd);
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

        char *cmd_buffer  = castack_push(memstack, 1, cat_str_size);

        /*put all the strings in the buffer*/
        for (size_t i = 0; i < 4; ++i)
                strcat(cmd_buffer, cmd_options[i]);

        if (-1 == system(cmd_buffer)) {
                eprintf("system(): Error occurred "
                        "while executing child process");
                exit(EXIT_FAILURE);
        }
         /*optional, since PW_MEMSTACK_ENABLE_AUTO_CLEAN macro is present*/
        castack_pop(memstack);
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

static void work_dispatch(unsigned wait_threshold, const char *process_name)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         *(same as config_parse_threshold())
         */
        const char *const pidof_filter = "pidof -x ";
        const char *const tr_filter = " | tr \' \' \'\n\'";
        size_t cat_str_size =
                strlen(pidof_filter) +
                strlen(process_name) +
                strlen(tr_filter)    + 1;
        char *cmd_buffer = castack_push(memstack, 1, cat_str_size);
        FILE *pidof_pipe = NULL;

        snprintf(cmd_buffer, cat_str_size, "%s%s%s",
                 pidof_filter,
                 process_name,
                 tr_filter);

        pidof_pipe = popen_or_die(cmd_buffer, "r");
        /*again, this is optional*/
        castack_pop(memstack);
        cmd_buffer = NULL;

        bool process_not_found = true;
        char linebuf[PW_LINEBUF_SIZE] = {};
        char *endptr = NULL;
        uintmax_t watched_process_id = 0;

        while (NULL != fgets(linebuf, sizeof linebuf, pidof_pipe)) {
                process_not_found = false;
                watched_process_id = strtoumax(linebuf, &endptr, 10);

                switch (fork_or_die()) {
                case 0:
                        process_monitor(wait_threshold,
                                        (pid_t)watched_process_id);
                        /*NEVER REACHED AGAIN*/
                        break;
                default:
                        break;
                }
        }
        if (true == process_not_found) {
                eprintf("Process not found\n");
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
                         *to kill the watched process
                         */
                        perror("kill()");
                        break;
                case ESRCH:
                        /*The watched process no longer exists*/
                        perror("kill()");
                        break;
                }
        }
        exit(EXIT_SUCCESS);
}

static void log_setup(void)
{
        /*
         *TODO
         *does the environment variable only specifies the path?
         */
        const char *const pwlog_location =secure_getenv_or_die("PROCNANNYLOGS");
        const char *const pwlog_name = "procnanny.log";
        size_t pwlog_path_len = strlen(pwlog_location) + strlen(pwlog_name) + 1;
        char *pwlog_path = castack_push(memstack, 1, pwlog_path_len);

        snprintf(pwlog_path, pwlog_path_len,
                 "%s%s", pwlog_location, pwlog_name);
        pwlogfd = open(pwlog_path, O_WRONLY | O_APPEND);
}

static void memstack_clean(void)
{
        castack_destroy(&memstack);
}

        /*
        if (castack_empty(castack_ptr))
                puts("It is empty!");
        int *arr_ptr = castack_push(castack_ptr, 4, sizeof(int));
        for (int i = 0; i < 4; ++i)
                printf("%d\t", arr_ptr[i]);
        putchar('\n');
        if (!castack_empty(castack_ptr))
                printf("Has %zu elements\n", castack_report(castack_ptr));
        castack_destroy(castack_ptr);
        */
