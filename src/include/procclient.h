#ifndef PROCCLIENT_H_
#define PROCCLIENT_H_

#include <sys/types.h>
/*
 *this macro is required to avoid making the array using it a VLA;
 *const size_t CHILD_READ_SIZE does not qualify as a
 *constant expression in C
 *(that is also the reason PW_LINEBUF_SIZE is declared as
 *a enumerator)
 */
#define PW_CHILD_READ_SIZE ((ssize_t)(sizeof(pid_t) + sizeof(unsigned)))

struct pw_idle_info {
        pid_t child_pid;
        int ipc_fdes[2];
};

#endif
