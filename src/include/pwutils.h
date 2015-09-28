#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

static void config_parse(int argc, char **argv);
static void config_parse_helper(FILE *const nanny_cfg);
static inline void clean_up(void);

#endif
