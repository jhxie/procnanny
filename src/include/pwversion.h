#ifndef PWVERSION_H_
#define PWVERSION_H_

#if defined (PROCWATCH_MAJOR) || defined (PROCWATCH_MINOR)
#undef PROCWATCH_MAJOR
#undef PROCWATCH_MINOR
#define PROCWATCH_MAJOR 0
#define PROCWATCH_MINOR 3
#endif

#endif
