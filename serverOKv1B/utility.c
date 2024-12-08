#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

/* readready() - check whether read ready for given file descriptor
* . return non-negative if ready, 0 if not ready, negative on errors
*/
int readready(int fd) {
    fd_set map;
    int ret;
    struct timeval _zerotimeval = {0, 0};
    do {
        FD_ZERO(&map); FD_SET(fd, &map);
        ret = select(fd+1, &map, NULL, NULL, &_zerotimeval);
        if(ret >= 0) return ret;
    } while(errno == EINTR);
    return ret;
}
/* readline() - read a line (ended with '\n') from a file descriptor
* . return the number of chars read, -1 on errors
*/
int readline(int fd, char *ptr, int maxlen) {
    int n, rc;
    char c;

    for (n = 1; n < maxlen; n++) {
        if ( (rc = read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if (c == '\n') break;
        } else if (rc == 0) {
            if (n == 1) return (0); /* EOF, no data read */
            else break; /* EOF, some data read */
        } else return (-1); /* Error */
    }
    *ptr = 0;
    return (n);
}

/* my_inet_addr() - convert host/IP into binary data in network byte 
order
*/
in_addr_t my_inet_addr(char *host) {
    in_addr_t inaddr;
    struct hostent *hp;

    inaddr = inet_addr(host);
    if(inaddr == INADDR_NONE && (hp = gethostbyname(host)) != NULL)
        bcopy(hp->h_addr, (char *) &inaddr, hp->h_length);
    return inaddr;
}
