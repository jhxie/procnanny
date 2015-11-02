#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

static void procclean(void);
static void work_dispatch(FILE *pwlog, struct pw_log_info *const loginfo);
static void process_monitor(unsigned wait_threshold, pid_t watched_process_id)
                __attribute__((noreturn));
static void pid_array_update(pid_t child_pid,
                             pid_t watched_pid,
                             const char *process_name,
                             unsigned wait_threshold);
static void pid_array_destroy(void);
#endif
