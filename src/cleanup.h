#include <curl/curl.h>

void curl_slistp_free_all(struct curl_slist** ptr);
void charpp_free(char** ptr);
void curlupp_free(CURLU** ptr);
void curlcharpp_free(char** ptr);

#pragma once