#ifndef DOWNLOAD_IMAGE_H
#define DOWNLOAD_IMAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *data);
void send_image(int client_sockfd);
void add_https_prefix(char* url);
int download_image(char *url);

#endif
