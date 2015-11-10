#ifndef PWLOG_H_
#define PWLOG_H_

#include <stdio.h>
#include <sys/types.h>

#include "procwatch.h"

FILE *pwlog_setup(void) __attribute__((warn_unused_result));
void pwlog_write(FILE *pwlog, struct pw_pid_info *loginfo);
#endif
