#if !defined (PWUTILS_H_) && defined (PW_ONLY)
#define PWUTILS_H_

#include <stdio.h>

enum { LINEBUF_SIZE = 1024 };

static void config_parse(int argc, char **argv);
static unsigned config_parse_threshold(FILE *const nanny_cfg);
static char *config_parse_pname(const char *const nanny_cfg_name);
static inline void clean_up(void);

#endif
