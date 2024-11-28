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
#include <signal.h>
#include <ctype.h>
#include "utility.h"

#define MAX_LINE 1024

void decode_url(char *url) {
    char *pos;
    // 循環查找並替換 %2F 為 /
    while ((pos = strstr(url, "%2F")) != NULL) {
        // 替換 %2F 為 /
        memmove(pos + 1, pos + 3, strlen(pos + 3) + 1); // 移動字符串
        *pos = '/'; // 替換 %2F 為 /
    }
}

// 解析 URL，提取主機名和路徑
void parse_url(const char *url, char *host, char *path) {
    decode_url(url);

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
    sprintf(request, "GET %s HTTP/1.0\r\n", path);
    strcat(request, "Host: ");
    strcat(request, host);
    strcat(request, "\r\n");
    strcat(request, "Connection: close\r\n");
    strcat(request, "\r\n");
    send(sockfd, request, strlen(request), 0);
}

// 提取 GET 請求中的 URL 編碼部分，去掉 "http%3A%2F%2F" 之前的部分
void extract_encoded_url(char *buf, char *encoded_url) {
    // 查找 GET 請求中的 "?url=" 部分
    char *url_start = strstr(buf, "GET /?url=");
    if (url_start) {
        url_start += 10;  // 跳過 "GET /?url=" 部分

        // 查找 URL 中的 "http%3A%2F%2F" 部分
        char *http_prefix = strstr(url_start, "http%3A%2F%2F");
        if (http_prefix) {
            // 跳過 "http%3A%2F%2F" 部分
            http_prefix += strlen("http%3A%2F%2F");
            // 直到空格或換行符為止提取 URL 的編碼部分
            char *url_end = strchr(http_prefix, ' ');
            if (url_end) {
                strncpy(encoded_url, http_prefix, url_end - http_prefix);
                encoded_url[url_end - http_prefix] = '\0';  // 確保字串結尾
            } else {
                // 如果沒有空格，說明 URL 是直到行結尾
                strcpy(encoded_url, http_prefix);
            }
        }
    }
}

// 處理來自瀏覽器的請求
void handle_client(int client_sockfd) {
    char buf[MAX_LINE];
    char host[MAX_LINE], path[MAX_LINE];
    char url[MAX_LINE];

    // 回傳簡單的 HTML 表單給使用者
    char *form_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body>"
        "<h1>My Proxy B1105129</h1>"
        "<form method='GET'>"
        "Enter URL: <input type='text' name='url' size='50'>"
        "<input type='submit' value='Fetch'>"
        "</form>";

    send(client_sockfd, form_response, strlen(form_response), 0);

    // 接收使用者輸入的 URL
    read(client_sockfd, buf, MAX_LINE);

    extract_encoded_url(buf, url);

    // 解析主機與路徑
    parse_url(url, host, path);

    // 與目標伺服器建立連線並取得內容
    int server_sockfd = tcp_open_client(host, "80");
    if (server_sockfd >= 0) {
        send_http_request(server_sockfd, host, path);

        // 回傳目標伺服器的內容
        while (1) {
            int n = recv(server_sockfd, buf, MAX_LINE, 0);
            if (n <= 0) break;
            send(client_sockfd, buf, n, 0);
        }
        close(server_sockfd);
    } else {
        char *error_msg =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n\r\n"
            "<html><body><h1>Failed to connect to the target server</h1></body></html>";
        send(client_sockfd, error_msg, strlen(error_msg), 0);
        send(client_sockfd, url, strlen(url), 0);
    }
    // http://gaia.cs.umass.edu/wireshark-labs/HTTP-wireshark-file1.html 

    close(client_sockfd);
}

// 啟動 Proxy Server
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int sockfd, newsockfd, clilen;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    listen(sockfd, 5);

    printf("Proxy server is running on port %s...\n", argv[1]);

    for (;;) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Accept failed");
            continue;
        }
        handle_client(newsockfd);
    }

    close(sockfd);
    return 0;
}