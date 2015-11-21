#if !defined (PWSUTILS_H_) && defined (PWSERVER_ONLY)
#define PWSUTILS_H_

static void procserver(void);
static void descriptors_inspect(const int listen_sockfd, const fd_set *readset);
static void clients_serve(const int listen_sockfd);
static void batchlog_write(const fd_set *clientset);
static void signal_handle(int sig);
#endif
