#ifndef PWLOG_H_
#define PWLOG_H_

#include <stdio.h>
#include <sys/types.h>

struct pw_log_info {
    enum { INFO_INIT, INFO_NOEXIST, INFO_REPORT, ACTION_KILL } log_type;
    pid_t watched_pid;
    unsigned   wait_threshold;
    const char *process_name;
    size_t num_term;
};

FILE *pwlog_setup(void) __attribute__((warn_unused_result));
void pwlog_write(FILE *pwlog, struct pw_log_info *loginfo);
#endif
