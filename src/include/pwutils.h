#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

#define WORK_DISPATCH_CHILD_CLEANUP() \
        do { \
                bst_destroy(&wpid_bst); \
                pclose(pidof_pipe); \
                fclose_or_die(pwlog); \
                close_or_die(tmp_fdes1[0]); \
                close_or_die(tmp_fdes2[1]); \
        } while (0)

#define WORK_DISPATCH_PARENT_CLEANUP() \
	do { \
		close_or_die(tmp_fdes1[1]); \
		close_or_die(tmp_fdes2[0]); \
		wpid_info.watched_pid = (pid_t)watched_pid; \
		wpid_info.child_pid = (pid_t)child_pid; \
		wpid_info.ipc_fdes[0] = tmp_fdes1[0]; \
		wpid_info.ipc_fdes[1] = tmp_fdes2[1]; \
	} while (0)

static void procclean(void);
static void work_dispatch(FILE *pwlog, const struct pw_cfg_info *const cfginfo);
static FILE *pidof_popenr(const char *const process_name)
        __attribute__((warn_unused_result));
static void process_monitor(pid_t    watched_process_id,
                            unsigned wait_threshold,
                            int      readdes,
                            int      writedes)
                __attribute__((noreturn));
#endif
