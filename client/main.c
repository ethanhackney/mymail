#include "../net.h"
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdlib.h>

static int do_connect(char *host, char *service);
static void chomp(char *s);
static void prompt(char *prompt, char *dest, size_t size);

int
main(void)
{
        char *host = "localhost";
        char *service = "8080";
        int connfd;
        char cmd[BUFSIZ];
        char email[BUFSIZ];
        char pword[BUFSIZ];
        char cmdline[sizeof(cmd) + sizeof(email) + sizeof(pword) + 1];
        char res[BUFSIZ + 1];
        ssize_t n;

        connfd = do_connect(host, service);

        for (;;) {
                printf("[[ sign in ]]\n");
                printf("[[ sign up ]]\n");
                printf("[[ exit    ]]\n");
                printf("=============\n");

                prompt("command", cmd, sizeof(cmd));
                chomp(cmd);

                if (!strcmp(cmd, "exit")) {
                        printf("cya:)\n");
                        exit(0);
                }

                if (strcmp(cmd, "sign in") &&
                    strcmp(cmd, "sign up")) {
                        printf("invalid command: %s\n", cmd);
                        continue;
                }

                prompt("email", email, sizeof(email));
                chomp(email);
                if (!strlen(email)) {
                        printf("need email to sign in with\n");
                        continue;
                }

                prompt("password", pword, sizeof(pword));
                chomp(pword);
                if (!strlen(pword)) {
                        printf("need password to sign in with\n");
                        continue;
                }

                snprintf(cmdline, sizeof(cmdline), "%s:%s:%s", 
                                cmd, email, pword);
                n = write(connfd, cmdline, strlen(cmdline));
                if (n != strlen(cmdline))
                        err(EX_OSERR, "server write failure");

                n = read(connfd, res, BUFSIZ);
                if (n <= 0)
                        err(EX_OSERR, "server read failure");
                res[n] = 0;
                chomp(res);
                if (!strncmp(res, "ERROR", 5)) {
                        char *msg = res + strlen("ERROR: ");
                        printf("sign in failed: %s\n", msg);
                } else {
                        printf("sign in successful\n");
                        break;
                }
        }

        if (close(connfd))
                err(EX_OSERR, "close");
}

static int
do_connect(char *host, char *service)
{
        struct addrinfo hints;
        struct addrinfo *infolist;
        int connfd;
        int error;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        error = getaddrinfo(host, service, &hints, &infolist);
        if (error)
                errx(EX_SOFTWARE, "getaddrinfo: %s", gai_strerror(error));

        connfd = net_connect(infolist);
        freeaddrinfo(infolist);
        infolist = NULL;
        if (connfd < 0)
                err(EX_SOFTWARE, "net_connect");

        return connfd;
}

static void
prompt(char *prompt, char *dest, size_t size)
{
        printf("[%s]> ", prompt);
        if (!fgets(dest, size, stdin)) {
                printf("cya:)\n");
                exit(0);
        }
}

static void
chomp(char *s)
{
        if (s[strlen(s) - 1] == '\n')
                s[strlen(s) - 1] = 0;
}
