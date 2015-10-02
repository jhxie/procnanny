#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

/*
 *note this is the only explicit call of castack_destroy():
 *for all the other cases castack_destroy() is guaranteed
 *to be called upon exit()
 */
#define PW_MEMSTACK_ENABLE_AUTO_CLEAN() do { \
        if (0 != atexit(clean_up)) { \
                eprintf("atexit(): cannot set exit function\n"); \
                castack_destroy(&memstack); \
                exit(EXIT_FAILURE); \
        } \
} while (0)

enum { LINEBUF_SIZE = 1024 };

static struct pw_config_info config_parse(int argc, char **argv);
static unsigned config_parse_threshold(FILE *const nanny_cfg);
static char *config_parse_pname(const char *const nanny_cfg_name);
static inline void clean_up(void);

#endif
