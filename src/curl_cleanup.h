#include <curl/curl.h>

void _curl_slist_free_all_(struct curl_slist** ptr);
void _curl_free_(char** ptr);
void _curl_url_cleanup_(CURLU** ptr);

#define __curl_slist_free_all__ __attribute__((__cleanup__(_curl_slist_free_all_)))
#define __curl_free__ __attribute__((__cleanup__(_curl_free_)))
#define __curl_url_cleanup__ __attribute__((__cleanup__(_curl_url_cleanup_)))