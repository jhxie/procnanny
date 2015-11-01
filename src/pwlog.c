#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "procwatch.h"
#include "pwlog.h"
#include "pwwrapper.h"

#include "memwatch.h"

FILE *pwlog_setup(void)
{
        const char *const pwlog_path =secure_getenv_or_die("PROCNANNYLOGS");

        return fopen_or_die(pwlog_path, "w");
}

void pwlog_write(FILE *pwlog, struct pw_log_info *loginfo)
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
        timestr = NULL;

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

