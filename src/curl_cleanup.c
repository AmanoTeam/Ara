#include <curl/curl.h>

#include "curl_cleanup.h"

void _curl_slist_free_all_(struct curl_slist** ptr) {
	curl_slist_free_all(*ptr);
}

void _curl_free_(char** ptr) {
	curl_free(*ptr);
}

void _curl_url_cleanup_(CURLU** ptr) {
	curl_url_cleanup(*ptr);
}