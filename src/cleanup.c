#include <stdlib.h>

#include <curl/curl.h>

void curl_slistp_free_all(struct curl_slist** ptr) {
	curl_slist_free_all(*ptr);
}

void charpp_free(char** ptr) {
	free(*ptr);
}

void curlupp_free(CURLU** ptr) {
	curl_url_cleanup(*ptr);
}

void curlcharpp_free(char** ptr) {
	curl_free(*ptr);
}
