#if !defined (CFGUTILS_H_) && defined (CFG_ONLY)
#define CFGUTILS_H_

#include <stdio.h>

static unsigned config_parse_threshold(FILE *const nanny_cfg);
static char *config_parse_pname(const char *const nanny_cfg_name);

#endif
