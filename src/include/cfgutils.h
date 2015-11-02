#if !defined (CFGUTILS_H_) && defined (CFG_ONLY)
#define CFGUTILS_H_

#include <stdio.h>

static char *config_parse_pname(char *const cfgline, char **saveptr);
static unsigned config_parse_threshold(char *const cfgline, char *saveptr);
#endif
