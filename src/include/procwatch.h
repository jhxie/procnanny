#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

void procwatch(int argc, char **argv);

#endif
