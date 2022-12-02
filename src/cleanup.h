#include <curl/curl.h>

#include "ttidy.h"

void curl_slistp_free_all(struct curl_slist** ptr);
void charpp_free(char** ptr);
void curlupp_free(CURLU** ptr);
void curlcharpp_free(char** ptr);
void tidy_releasep(const tidy_doc_t* const* ptr);

#pragma once
