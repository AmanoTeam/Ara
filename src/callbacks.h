#include "types.h"
#include "utils.h"

size_t curl_write_string_cb(char *chunk, size_t size, size_t nmemb, struct String* string);
size_t curl_progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
size_t curl_write_file_cb(char *chunk, size_t size, size_t nmemb, struct FStream* stream);

#pragma once
