#include <curl/curl.h>

CURL* get_global_curl_easy(void);
CURL* curl_easy_new(void);

CURLM* get_global_curl_multi(void);

const char* get_global_curl_error(void);

CURLcode curl_easy_perform_retry(CURL* const curl);

void __curl_slist_free_all(struct curl_slist** ptr);
void __curl_free(char** ptr);
void __curl_url_cleanup(CURLU** ptr);

#define __curl_slist_free_all__ __attribute__((__cleanup__(__curl_slist_free_all)))
#define __curl_free__ __attribute__((__cleanup__(__curl_free)))
#define __curl_url_cleanup__ __attribute__((__cleanup__(__curl_url_cleanup)))