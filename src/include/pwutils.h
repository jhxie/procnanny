#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

/*
 *note this is the only explicit call of castack_destroy():
 *for all the other cases castack_destroy() is guaranteed
 *to be called upon exit()
 */
#define PW_MEMSTACK_ENABLE_AUTO_CLEAN() do { \
        if (0 != atexit(memstack_clean)) { \
                eprintf("atexit(): cannot set exit function\n"); \
                castack_destroy(&memstack); \
                exit(EXIT_FAILURE); \
        } \
} while (0)

static void procclean(void);
static struct pw_config_info config_parse(int argc, char **argv);
static unsigned config_parse_threshold(FILE *const nanny_cfg);
static char *config_parse_pname(const char *const nanny_cfg_name);
static void work_dispatch(FILE *pwlog, struct pw_log_info *const loginfo);
static void process_monitor(unsigned wait_threshold, pid_t watched_process_id)
                __attribute__((noreturn));
static FILE *pwlog_setup(void) __attribute__((warn_unused_result));
static void pwlog_write(FILE *pwlog, struct pw_log_info *loginfo);
static void pid_array_update(pid_t child_pid,
                             pid_t watched_pid,
                             const char *process_name);
static inline void memstack_clean(void);

#endif
