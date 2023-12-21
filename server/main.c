#include "../net.h"
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <fcntl.h>

struct user {
        int     u_connfd;
        int     u_mailfd;
        int     u_blkfd;
        int     u_sentfd;
        int     u_infofd;
};

static int do_listen(char *host, char *service, int qsize);
static void sigchld_handler(int signo);
static void login(int connfd);

int
main(void)
{
        struct sigaction act;
        char *host = "localhost";
        char *service = "8080";
        pid_t pid;
        int qsize = 10;
        int listenfd;
        int connfd;

        memset(&act, 0, sizeof(act));
        act.sa_flags = 0;
        act.sa_handler = sigchld_handler;
        sigemptyset(&act.sa_mask);
        if (sigaction(SIGCHLD, &act, NULL))
                err(EX_OSERR, "sigaction");

        listenfd = do_listen(host, service, qsize);

        for (;;) {
                connfd = accept(listenfd, NULL, NULL);
                if (connfd < 0 && errno == EINTR)
                        continue;
                if (connfd < 0)
                        err(EX_OSERR, "accept");

                pid = fork();
                if (pid < 0) {
                        err(EX_OSERR, "fork");
                } else if (!pid) {
                        if (close(listenfd))
                                err(EX_OSERR, "close");
                        login(connfd);
                        exit(0);
                }

                if (close(connfd))
                        err(EX_OSERR, "close");
        }
}

static int
do_listen(char *host, char *service, int qsize)
{
        struct addrinfo hints;
        struct addrinfo *infolist;
        int listenfd;
        int error;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        error = getaddrinfo(host, service, &hints, &infolist);
        if (error)
                errx(EX_SOFTWARE, "getaddrinfo: %s", gai_strerror(error));

        listenfd = net_listen(infolist, qsize);
        freeaddrinfo(infolist);
        infolist = NULL;
        if (listenfd < 0)
                err(EX_SOFTWARE, "net_listen");

        return listenfd;
}

static void
sigchld_handler(int signo)
{
        pid_t pid;

        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
                ;

        if (pid < 0 && errno != ECHILD)
                err(EX_OSERR, "waitpid");
}

static void fd_puts(int fd, char *s);
static void client_sh(struct user *up);
static void kill_user(char *upath);

static void
login(int connfd)
{
        char cmdline[(BUFSIZ * 3) + 1];
        char *cmdp;
        char *cmd;
        char *email;
        char *pword;
        ssize_t n;

        n = read(connfd, cmdline, (BUFSIZ * 3));
        if (n <= 0)
                err(EX_OSERR, "client failure");
        cmdline[n] = 0;

        cmdp = cmdline;
        cmd = strsep(&cmdp, ":");
        email = strsep(&cmdp, ":");
        pword = strsep(&cmdp, "\n");

        if (!strcmp(cmd, "sign in")) {
                struct stat stats;
                struct user u;
                char path[BUFSIZ] = "./users/";
                char *end;
                char saved_pword[BUFSIZ + 1];

                strcat(path, email);
                printf("user dirpath: %s\n", path);
                if (stat(path, &stats)) {
                        fd_puts(connfd, "ERROR: user does not exist");
                        return;
                }

                strcat(path, "/");
                end = path + strlen(path);
                strcpy(end, "mail");
                printf("user mailpath: %s\n", path);
                u.u_mailfd = open(path, O_RDWR, 0666);
                if (u.u_mailfd < 0) {
                        fd_puts(connfd, "ERROR: could not open user mail file");
                        return;
                }

                strcpy(end, "blocked");
                printf("user blkpath: %s\n", path);
                u.u_blkfd = open(path, O_RDWR, 0666);
                if (u.u_blkfd < 0) {
                        fd_puts(connfd, "ERROR: could not open user blocked user file");
                        return;
                }

                strcpy(end, "sent");
                printf("user sentpath: %s\n", path);
                u.u_sentfd = open(path, O_RDWR, 0666);
                if (u.u_sentfd < 0) {
                        fd_puts(connfd, "ERROR: could not open user sent mail file");
                        return;
                }

                strcpy(end, "info");
                printf("user infopath: %s\n", path);
                u.u_infofd = open(path, O_RDWR, 0666);
                if (u.u_infofd < 0) {
                        fd_puts(connfd, "ERROR: could not open user info file");
                        return;
                }

                n = read(u.u_infofd, saved_pword, BUFSIZ);
                if (n <= 0) {
                        fd_puts(connfd, "ERROR: could not read user info file");
                        return;
                }
                saved_pword[n] = 0;
                if (strcmp(pword, saved_pword)) {
                        fd_puts(connfd, "ERROR: invalid password");
                        return;
                }

                u.u_connfd = connfd;
                client_sh(&u);
        } else {
                struct user u;
                char path[BUFSIZ] = "./users/";
                char *end;

                strcat(path, email);
                printf("user dirpath: %s\n", path);
                if (mkdir(path, 0775)) {
                        fd_puts(connfd, "ERROR: could not create user");
                        return;
                }

                strcat(path, "/");
                end = path + strlen(path);
                strcpy(end, "mail");
                printf("user mailpath: %s\n", path);
                u.u_mailfd = open(path, O_CREAT | O_RDWR, 0666);
                if (u.u_mailfd < 0) {
                        fd_puts(connfd, "ERROR: could not create user mail file");
                        kill_user(dirname(path));
                        return;
                }

                strcpy(end, "blocked");
                printf("user blkpath: %s\n", path);
                u.u_blkfd = open(path, O_CREAT | O_RDWR, 0666);
                if (u.u_blkfd < 0) {
                        fd_puts(connfd, "ERROR: could not create user blocked user file");
                        kill_user(dirname(path));
                        return;
                }

                strcpy(end, "sent");
                printf("user sentpath: %s\n", path);
                u.u_sentfd = open(path, O_CREAT | O_RDWR, 0666);
                if (u.u_sentfd < 0) {
                        fd_puts(connfd, "ERROR: could not create user sent mail file");
                        kill_user(dirname(path));
                        return;
                }

                strcpy(end, "info");
                u.u_infofd = open(path, O_CREAT | O_RDWR, 0666);
                if (u.u_infofd < 0) {
                        fd_puts(connfd, "ERROR: could not create user info file");
                        kill_user(dirname(path));
                        return;
                }
                n = write(u.u_infofd, pword, strlen(pword));
                if (n != (ssize_t)strlen(pword)) {
                        fd_puts(connfd, "ERROR: could not initialize user info file");
                        kill_user(dirname(path));
                        return;
                }

                u.u_connfd = connfd;
                client_sh(&u);
        }
}

static void
fd_puts(int fd, char *s)
{
        if (write(fd, s, strlen(s)) != (ssize_t)strlen(s))
                err(EX_OSERR, "fd_puts");
}

static void
client_sh(struct user *up)
{
        char res[BUFSIZ] = "OK";
        ssize_t n = strlen(res);

        if (write(up->u_connfd, res, n) != n)
                err(EX_OSERR, "client failure");
}

static void
kill_user(char *upath)
{
        char cmd[BUFSIZ];

        strcpy(cmd, "rm -rf ");
        strcat(cmd, upath);
        system(cmd);
}
