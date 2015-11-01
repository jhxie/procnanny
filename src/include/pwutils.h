#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

static void procclean(void);
static struct pw_config_info config_parse(int argc, char **argv);
static unsigned config_parse_threshold(FILE *const nanny_cfg);
static char *config_parse_pname(const char *const nanny_cfg_name);
static void work_dispatch(FILE *pwlog, struct pw_log_info *const loginfo);
static void process_monitor(unsigned wait_threshold, pid_t watched_process_id)
                __attribute__((noreturn));
static void pid_array_update(pid_t child_pid,
                             pid_t watched_pid,
                             const char *process_name);
static void pid_array_destroy(void);
#endif
