#if !defined (PWCUTILS_H_) && defined (PWCLIENT_ONLY)
#define PWCUTILS_H_

#include <stdio.h>

static void procwatch(void);
static void work_dispatch(const struct pw_cfg_info *cfginfo, fd_set *chksetptr);
static void parent_msg_write(struct pw_pid_info *const wpid_info);
static void child_create(FILE *pidof_pipe, struct pw_pid_info *const wpid_info);
static void process_monitor(int readdes, int writedes)
                __attribute__((noreturn));
static FILE *pidof_popenr(const char *const process_name)
        __attribute__((warn_unused_result));
static void clean_up(void);
#endif
