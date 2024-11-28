#ifndef UTILITY_H
#define UTILITY_H

#include <netinet/in.h>

int readready(int fd);
int readline(int fd, char *ptr, int maxlen);
in_addr_t my_inet_addr(char *host);

#endif