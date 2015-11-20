#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <limits.h>
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

FILE *pwlog_info(void)
{
        const char *const pwinfo_path =
                secure_getenv_or_die("PROCNANNYSERVERINFO");

        return fopen_or_die(pwinfo_path, "w");
}

void pwlog_write(FILE *pwlog, struct pw_pid_info *loginfo)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         */
        char hostname[HOST_NAME_MAX + 1] = { 0 };
        /*
         *POSIX.1 guarantees that "Host names (not including the terminating
         *null byte) are limited to HOST_NAME_MAX  bytes"
         */
        gethostname_or_die(hostname, HOST_NAME_MAX + 1);

        char timebuf[PW_LINEBUF_SIZE]       = { 0 };
        time_t epoch_time = time(NULL);
        struct tm *cal = localtime(&epoch_time);
        strftime(timebuf, PW_LINEBUF_SIZE, "%a %b %d %T %Z %Y", cal);

        /*Note the constant reserves space for both square brackets*/
        char *timestr = calloc_or_die(1, strlen(timebuf) + 3);
        snprintf(timestr, strlen(timebuf) + 3, "[%s]", timebuf);
        /*Print the time field only when the type of pw_pid_info
         *is NOT INFO_STARTUP
         */
        if (INFO_STARTUP != loginfo->type) {
                fputs_or_die(timestr, pwlog);
        }

        /*
         *For the pid_t data type, POSIX standard                    
         *guarantees that it will fit in a long integer,so we cast it 
         */
        switch (loginfo->type) {

        case INFO_STARTUP:
                fprintf(pwlog,
                        "NODE %s PID %ld PORT %d\n",
                        hostname, (long)getpid(), PW_SERVER_PORT_NUM);
                fprintf(stdout,
                        "%s procnanny server: PID %ld on node %s, port %d\n",
                        timestr, (long)getpid(), hostname, PW_SERVER_PORT_NUM);
                break;
        case INFO_INIT:
                fprintf(pwlog,
                        " Info: Initializing monitoring of "
                        "process \'%s\' (PID %ld) on node %s.\n",
                        loginfo->process_name, (long)loginfo->watched_pid);
                break;
        case INFO_NOEXIST:
                fprintf(pwlog, " Info: No \'%s\' processes found on %s.\n",
                        loginfo->process_name);
                break;
        case INFO_REPORT:
                fprintf(pwlog,
                        " Info: Caught SIGINT. Exiting cleanly. %zu "
                        "process(es) killed on ",
                        num_killed);
                fputs_or_die(timestr, stdout);
                fprintf(stdout,
                        " Info: Caught SIGINT. Exiting cleanly. %zu "
                        "process(es) killed on ",
                        num_killed);
                break;
        case INFO_REREAD:
                fprintf(pwlog,
                        "  Info: Caught SIGHUP. "
                        "Configuration file \'%s\' re-read.\n",
                        configname);
                fputs_or_die(timestr, stdout);
                fprintf(stdout,
                        "  Info: Caught SIGHUP. "
                        "Configuration file \'%s\' re-read.\n",
                        configname);
                break;
        case ACTION_KILL:
                fprintf(pwlog,
                        " Action: PID %ld (%s) on %s killed "
                        "after exceeding %u seconds.\n",
                        (long)loginfo->watched_pid,
                        loginfo->process_name,
                        loginfo->cwait_threshold);
                break;
        }
        zerofree(timestr);
        fflush(stdout);
        fflush(pwlog);
}

