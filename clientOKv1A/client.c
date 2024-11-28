#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "utility.h"

#define MAX_LINE 1024

int tcp_open_client(char *host, char *port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = my_inet_addr(host);
    serv_addr.sin_port = htons(atoi(port));

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
        connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        return -1;

    return sockfd;
}

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    int sockfd, len, ret;
    char buf[MAX_LINE];

    sockfd = tcp_open_client(argv[1], argv[2]);
    if (sockfd < 0) {
        perror("Failed to connect to the server");
        return 1;
    }

    while (1) {
        if ((ret = readready(sockfd)) < 0) break;
        if (ret > 0) {
            if (readline(sockfd, buf, MAX_LINE) <= 0) break;
            fputs(buf, stdout);
        }
        if ((ret = readready(0)) < 0) break;
        if (ret > 0) {
            if (fgets(buf, MAX_LINE, stdin) == NULL) break;
            len = strlen(buf);
            send(sockfd, buf, len, 0);
            if (strncmp(buf, "QUIT", 4) == 0) break;
        }
    }
    close(sockfd);
    return 0;
}