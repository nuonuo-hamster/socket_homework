#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <curl/curl.h>

// 寫入回應數據的回調函數
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *data) {
    size_t written = fwrite(ptr, size, nmemb, data);
    return written;
}

void send_image(int client_sockfd, const char *image_url) {
    
    char form_response[4096];
    snprintf(form_response, sizeof(form_response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body>"
        "<h2>Displayed Image</h2>"
        "<img src='%s' alt='Website Image' width='200' height='300' style='border:1px solid black;'/>"
        "</body></html>", image_url);

    send(client_sockfd, form_response, strlen(form_response), 0);
}

void add_https_prefix(char* url) {
    // 檢查 URL 是否以 "http://" 或 "https://" 開頭
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        // 如果沒有前綴，添加 "https://"
        char new_url[256];
        snprintf(new_url, sizeof(new_url), "https://%s", url);
        strcpy(url, new_url);
    }
}

int download_image(char *url) {
    CURL *curl;
    CURLcode res;
    FILE *fp;

    // 添加 https:// 前綴
    add_https_prefix(url);

    // 開啟檔案，準備寫入圖片
    fp = fopen("downloaded_image.jpg", "wb");
    if (fp == NULL) {
        perror("無法開啟檔案");
        return 1;
    }

    // 初始化libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // 設置要請求的URL
        curl_easy_setopt(curl, CURLOPT_URL, url);
        // 設置回應數據的寫入回調函數
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

        // 執行HTTP請求
        res = curl_easy_perform(curl);

        // 檢查是否發生錯誤
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform()失敗: %s\n", curl_easy_strerror(res));
        } else {
            printf("圖片下載並儲存成功！\n");
        }

        // 釋放資源
        curl_easy_cleanup(curl);
    }

    // 關閉檔案
    fclose(fp);

    // 清理libcurl
    curl_global_cleanup();

    return 0;
}
