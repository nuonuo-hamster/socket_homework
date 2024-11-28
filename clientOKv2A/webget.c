#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "utility.h"

#define MAX_LINE 1024

// 解析 URL，提取主機名和路徑
void parse_url(const char *url, char *host, char *path) {
    const char *prefix = "http://";
    if (strncmp(url, prefix, strlen(prefix)) == 0) {
        url += strlen(prefix); // 跳過 "http://"
    }

    char *slash_pos = strchr(url, '/'); // 找到第一個 '/'
    if (slash_pos) {
        strncpy(host, url, slash_pos - url); // 提取主機名
        host[slash_pos - url] = '\0';       // 確保以 '\0' 結束
        strcpy(path, slash_pos);            // 提取路徑
    } else {
        strcpy(host, url);  // 如果沒有路徑，只取主機名
        strcpy(path, "/");  // 默認路徑為 "/"
    }
}

// 打開 TCP 連接
int tcp_open_client(char *host, char *port) { 
    int sockfd;
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = my_inet_addr(host);
    serv_addr.sin_port = htons(atoi(port));

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0
        || connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        return -1;
    return sockfd;
}

// 發送 HTTP GET 請求
void send_http_request(int sockfd, const char *host, const char *path) {
    char request[MAX_LINE];
    sprintf(request, "GET %s HTTP/1.0\r\n", path);    // 使用動態的路徑
    strcat(request, "Host: ");
    strcat(request, host);
    strcat(request, "\r\n");
    strcat(request, "Connection: close\r\n");
    strcat(request, "\r\n");  // 空行表示 HTTP 請求結束
    send(sockfd, request, strlen(request), 0);
}

int main(int argc, char *argv[]) {
    int sockfd, ret;
    char buf[MAX_LINE];
    char host[MAX_LINE], path[MAX_LINE];

    // 確保命令行參數有效
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <url>\n", argv[0]);
        exit(1);
    }

    // 解析 URL
    parse_url(argv[1], host, path);

    // 預設使用 HTTP 端口 80
    sockfd = tcp_open_client(host, "80");
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }
    
    // 發送 HTTP GET 請求
    send_http_request(sockfd, host, path);

    // 接收並顯示回應
    while (1) {
        if ((ret = readready(sockfd)) < 0) break;
        if (ret > 0) {
            if (readline(sockfd, buf, MAX_LINE) <= 0) break;
            fputs(buf, stdout);
        }
    }
    close(sockfd);
    return 0;
}
