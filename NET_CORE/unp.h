#ifndef __unp_h
#define __unp_h

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>

#define MAXLINE 4096
#define LISTENQ 1024

// Shortcut for struct sockaddr
#define SA struct sockaddr

/* Error handling functions (error.c) */
void err_quit(const char *, ...);
void err_sys(const char *, ...);

/* Network wrapper functions (wrapsock.c) */
int Accept(int, SA *, socklen_t *);
void Bind(int, const SA *, socklen_t);
void Listen(int, int);
int Select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int Socket(int, int, int);
ssize_t Send(int, const void *, size_t, int);
ssize_t Recv(int, void *, size_t, int);

/* System wrapper functions (wrapunix.c) */
void Close(int);
pid_t Fork(void);

#endif
