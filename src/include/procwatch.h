#ifndef PROCWATCH_H_
#define PROCWATCH_H_

#include <stdio.h>
#include <sys/types.h>

#include "pwwrapper.h"

/*
 *this macro is required to avoid making the array using it a VLA;
 *const size_t CHILD_READ_SIZE does not qualify as a
 *constant expression in C
 *(that is also the reason PW_LINEBUF_SIZE is declared as
 *a enumerator)
 */
#define PW_CHILD_READ_SIZE ((ssize_t)(sizeof(pid_t) + sizeof(unsigned)))

struct pw_pid_info {
        enum { INFO_INIT, INFO_NOEXIST, INFO_REPORT, ACTION_KILL } type;
        pid_t watched_pid;
        pid_t child_pid;
        unsigned cwait_threshold;
        unsigned pwait_threshold;
        char process_name[PW_LINEBUF_SIZE];
        int ipc_fdes[2];
};

struct pw_idle_info {
        pid_t child_pid;
        int ipc_fdes[2];
};

extern size_t num_killed;

void procwatch(const char *const cfgname);

#endif
