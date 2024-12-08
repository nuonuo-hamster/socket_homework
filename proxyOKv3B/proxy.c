#include <string.h>
#include <stdbool.h>
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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "download_image.c"

#define MAX_LINE 4096

// 初始化 OpenSSL
void init_openssl() {
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

// 创建 SSL 上下文
SSL_CTX *create_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // 僅啟用支持的 TLS 版本
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        perror("Failed to set minimum protocol version");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    // 設定憑證存儲路徑（CA 憑證）
    if (!SSL_CTX_load_verify_locations(ctx, "/etc/ssl/certs/ca-certificates.crt", NULL)) {
        perror("Failed to load CA certificates");
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }

    // 啟用憑證驗證
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    return ctx;
}

// 打开 TCP 连接
int tcp_open_client(const char *host, const char *port) {
    printf("Trying to connect to host: %s, port: %s\n", host, port);

    int sockfd;
    struct addrinfo hints, *res, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo failed");
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(sockfd);
    }

    freeaddrinfo(res);

    if (p == NULL) {
        perror("Connection failed");
        return -1;
    }

    return sockfd;
}

// 发送 HTTP GET 请求
void send_http_request(SSL *ssl, const char *host, const char *path) {
    char request[MAX_LINE];
    sprintf(request, "GET %s HTTP/1.1\r\n", path);
    strcat(request, "Host: ");
    strcat(request, host);
    strcat(request, "\r\n");
    strcat(request, "Connection: close\r\n");
    strcat(request, "\r\n");
    SSL_write(ssl, request, strlen(request));
}

// 提取 GET 请求中的 URL
void extract_encoded_url(char *buf, char *url) {
    char *url_start = strstr(buf, "GET /?url=");
    if (url_start) {
        
        url_start += 10;  // 跳过 "GET /?url="
             // 查找 URL 中的 "http%3A%2F%2F" 部分
        char *http_prefix = strstr(url_start, "https%3A%2F%2F");

        if (http_prefix) {
            // 跳過 "http%3A%2F%2F" 部分
            http_prefix += strlen("https%3A%2F%2F");
            // 直到空格或換行符為止提取 URL 的編碼部分
            char *url_end = strchr(http_prefix, ' ');
            if (url_end) {
                strncpy(url, http_prefix, url_end - http_prefix);
                url[url_end - http_prefix] = '\0';  // 確保字串結尾
            } else {
                // 如果沒有空格，說明 URL 是直到行結尾
                strcpy(url, http_prefix);
            }
        }
    }
}

void decode_url(char *url) {
    char *pos;
    // 循環查找並替換 %2F 為 /
    while ((pos = strstr(url, "%2F")) != NULL) {
        // 替換 %2F 為 /
        memmove(pos + 1, pos + 3, strlen(pos + 3) + 1); // 移動字符串
        *pos = '/'; // 替換 %2F 為 /
    }
    // 循環查找並替換 %2F 為 /
    while ((pos = strstr(url, "%3F")) != NULL) {
        // 替換 %2F 為 /
        memmove(pos + 1, pos + 3, strlen(pos + 3) + 1); // 移動字符串
        *pos = '?'; // 替換 %2F 為 /
    }
    // 循環查找並替換 %2F 為 /
    while ((pos = strstr(url, "%3D")) != NULL) {
        // 替換 %2F 為 /
        memmove(pos + 1, pos + 3, strlen(pos + 3) + 1); // 移動字符串
        *pos = '='; // 替換 %2F 為 /
    }
}

// 解析 URL
void parse_url(char *url, char *host, char *path) {
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

void REextract_encoded_url(char *buf, char *url) {
    char *url_start = strstr(buf, "HTTP/1.1 302");
    if (url_start) {
        // 查找 URL 中的 "http%3A%2F%2F" 部分
        char *http_prefix = strstr(url_start, "location: ");

        if (http_prefix) {
            // 跳過 "http%3A%2F%2F" 部分
            http_prefix += strlen("location: https://");
            // 直到空格或換行符為止提取 URL 的編碼部分
            char *url_end = strchr(http_prefix, '\r');
            if (url_end) {
                strncpy(url, http_prefix, url_end - http_prefix);
                url[url_end - http_prefix] = '\0';  // 確保字串結尾
            } else {
                // 如果沒有空格，說明 URL 是直到行結尾
                strcpy(url, http_prefix);
            }
        }
    }
}

void REparse_url(char *url, char *host, char *path) {
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

// 处理客户端请求
void handle_client(int client_sockfd, SSL_CTX *ctx) {
    char buf[MAX_LINE], host[MAX_LINE], path[MAX_LINE], url[MAX_LINE];

    // 返回 HTML 表单
    char *form_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body>"
        "<h1>B1105129 HTTPS Proxy Server</h1>"
        "<form method='GET'>"
        "Enter URL: <input type='text' name='url' size='50'>"
        "<input type='submit' value='Fetch'>"
        "</form>"
        "<h2>Fetch https://picsum.photos/200/300?random=1</h2>"
        "<img src='downloaded_image.jpg' alt='Downloaded Image' />"
        "</body></html>";
    send(client_sockfd, form_response, strlen(form_response), 0);

    // 接收用户输入
    read(client_sockfd, buf, sizeof(buf));
    extract_encoded_url(buf, url);

    // 解析 URL
    parse_url(url, host, path);
    printf("\nurl------------");
    printf("%s",url);
    printf("\nhost------------");
    printf("%s",host);
    printf("\npath------------");
    printf("%s",path);
    printf("\nDEBUG------------");
    // 打开 TCP 连接
    int server_sockfd = tcp_open_client(host, "443");
    if (server_sockfd < 0) {
        perror("Unable to connect to server");
        close(client_sockfd);
        return;
    }
    // 创建 SSL 连接
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server_sockfd);
    SSL_set_tlsext_host_name(ssl, host);
    if (SSL_connect(ssl) <= 0) {
        perror("SSL connection failed");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(server_sockfd);
        close(client_sockfd);
        return;
    }

    // 发送 HTTPS 请求
    send_http_request(ssl, host, path);

    // 转发&print目标服务器内容
    int n;
    while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0) {
        // 转发数据到客户端
        send(client_sockfd, buf, n, 0);

        // 确保在 buf 的末尾添加 null 字符来正确打印
        buf[n] = '\0';  // 添加 null 结尾字符

        // 打印接收到的数据
        printf("%s", buf);
    }

    // -------------------再次解析 URL

    REextract_encoded_url(buf, url);
    REparse_url(url, host, path);
    printf("\nurl------------");
    printf("%s",url);
    printf("\nhost------------");
    printf("%s",host);
    printf("\npath------------");
    printf("%s",path);
    printf("\nDEBUG------------");
    
    //下載jpg
    download_image(url);
    // send to server
    send_image(client_sockfd, url);

    // 清理资源
    // 清空字符陣列
    memset(buf, 0, sizeof(buf));
    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));
    memset(url, 0, sizeof(url));
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(server_sockfd);
    close(client_sockfd);
}

// 主函数
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    init_openssl();
    SSL_CTX *ctx = create_context();

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);
    printf("Proxy server is running on port %s...\n", argv[1]);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Accept failed");
            continue;
        }
        handle_client(newsockfd, ctx);
    }

    close(sockfd);
    SSL_CTX_free(ctx);
    return 0;
}