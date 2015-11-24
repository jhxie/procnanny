#if !defined (PWSUTILS_H_) && defined (PWSERVER_ONLY)
#define PWSUTILS_H_

static void procserver(void);
static bool fdset_check(const int listen_sockfd, fd_set *readset);
static void clients_serve(const int listen_sockfd, fd_set *readset);
static void signal_handle(int sig);
static void clean_up(void);
#endif
