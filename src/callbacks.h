#include <stdlib.h>

#include <curl/curl.h>

size_t curl_write_string_cb(char* ptr, size_t size, size_t nmemb, void* userdata);
size_t curl_progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
size_t curl_write_file_cb(char* ptr, size_t size, size_t nmemb, void* userdata);
size_t curl_discard_body_cb(char* ptr, size_t size, size_t nmemb, void* userdata);
size_t json_load_cb(void* buffer, size_t buflen, void* data);
int json_dump_cb(const char *buffer, size_t size, void* data);

#pragma once
