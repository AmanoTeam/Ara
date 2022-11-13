#include <curl/curl.h>

CURL* curl_easy_global(void);
CURL* curl_easy_new(void);

CURLM* curl_multi_global(void);
