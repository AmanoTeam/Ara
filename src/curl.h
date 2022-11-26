#include <curl/curl.h>

CURL* get_global_curl_easy(void);
CURL* curl_easy_new(void);

CURLM* get_global_curl_multi(void);
