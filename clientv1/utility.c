#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>    // for inet_ntop
#include <netdb.h>         // for getaddrinfo and addrinfo
#include <stdlib.h>

int readready(int fd) {
    fd_set map;
    struct timeval timeout = {0, 0};
    FD_ZERO(&map);
    FD_SET(fd, &map);

    int ret = select(fd + 1, &map, NULL, NULL, &timeout);
    if (ret < 0 && errno == EINTR) {
        return -1; // Interrupted by signal
    }
    return ret;
}

int readline(int fd, char *ptr, int maxlen) {
    int n, rc;
    char c;
    for (n = 1; n < maxlen; n++) {
        if ((rc = read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if (c == '\n') break;
        } else if (rc == 0) {
            if (n == 1) return 0; // EOF, no data read
            else break; // EOF, some data read
        } else return -1; // Error
    }
    *ptr = 0;
    return n;
}

in_addr_t my_inet_addr(char *host) {
    struct addrinfo hints, *res;
    struct sockaddr_in *addr;
    in_addr_t inaddr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Error: Unable to resolve host %s\n", host);
        exit(EXIT_FAILURE);
    }

    addr = (struct sockaddr_in *)res->ai_addr;
    inaddr = addr->sin_addr.s_addr;

    freeaddrinfo(res);
    return inaddr;
}
