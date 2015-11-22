#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "procserver.h"
#include "pwlog.h"
#include "pwwrapper.h"

#include "memwatch.h"

FILE *pwlog_setup(void)
{
        const char *const pwlog_path = secure_getenv_or_die("PROCNANNYLOGS");

        return fopen_or_die(pwlog_path, "w");
}

/*
 *According to the assignment specification:
 *"
 *When the server process nanny starts up, it must output its PID, the name of
 *the workstation it is running on, and the port number used by the server to
 *accept new socket connections.
 *Also, the procnanny.server process must print to a file, specified via
 *environment variable PROCNANNYSERVERINFO, with a single line of text output
 *"
 */
static FILE *pwlog_info(void)
{
        const char *const pwinfo_path =
                secure_getenv_or_die("PROCNANNYSERVERINFO");

        return fopen_or_die(pwinfo_path, "w");
}

void pwlog_write(FILE *pwlog, struct pw_pid_info *loginfo, const char *chost)
{
        /*
         *ASSUMPTION: the length of a line is no more than 1023 characters
         */
        char shost[HOST_NAME_MAX + 1] = { 0 };
        FILE *pwinfo = NULL;
        /*
         *POSIX.1 guarantees that "Host names (not including the terminating
         *null byte) are limited to HOST_NAME_MAX  bytes"
         */
        gethostname_or_die(shost, HOST_NAME_MAX + 1);

        char timebuf[PW_LINEBUF_SIZE]       = { 0 };
        time_t epoch_time = time(NULL);
        struct tm *cal = localtime(&epoch_time);
        strftime(timebuf, PW_LINEBUF_SIZE, "%a %b %d %T %Z %Y", cal);

        /*Note the constant reserves space for both square brackets*/
        char *timestr = calloc_or_die(1, strlen(timebuf) + 3);
        snprintf(timestr, strlen(timebuf) + 3, "[%s]", timebuf);
        /*Print the time field only*/
        fputs_or_die(timestr, pwlog);
        if (INFO_STARTUP == loginfo->type) {
                pwinfo = pwlog_info();
        }

        /*
         *For the pid_t data type, POSIX standard                    
         *guarantees that it will fit in a long integer,so we cast it 
         */
        switch (loginfo->type) {

        case INFO_STARTUP:
                fprintf(pwinfo,
                        "NODE %s PID %ld PORT %d\n",
                        shost, (long)getpid(), PW_SERVER_PORT_NUM);
                fprintf(pwlog,
                        " procnanny server: PID %ld on node %s, port %d\n",
                        (long)getpid(), shost, PW_SERVER_PORT_NUM);
                break;
        case INFO_INIT:
                fprintf(pwlog,
                        " Info: Initializing monitoring of "
                        "process \'%s\' (PID %ld) on node %s.\n",
                        loginfo->process_name,
                        (long)loginfo->watched_pid,
                        chost);
                break;
        case INFO_NOEXIST:
                fprintf(pwlog, " Info: No \'%s\' processes found on %s.\n",
                        loginfo->process_name, chost);
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
                        chost,
                        loginfo->cwait_threshold);
                break;
        }

        if (NULL != pwinfo) {
                fflush(pwinfo);
                fclose_or_die(pwinfo);
        }
        zerofree(timestr);
        fflush(stdout);
        fflush(pwlog);
}

