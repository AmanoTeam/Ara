#include <string.h>

#include <curl/curl.h>

#include "curl.h"
#include "cacert.h"

static const char HTTP_DEFAULT_USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36";

static CURL* curl = NULL;
static CURLM* curl_multi = NULL;

static struct curl_blob blob = {
	.data = (char*) CACERT
};

static int curl_globals_initialized = 0;

static void curl_init_globals(void) {
	
	if (!curl_globals_initialized) {
		curl_global_init(CURL_GLOBAL_ALL);
		curl_globals_initialized = 1;
	}
	
}


static void curl_set_options(CURL* handle) {
	
	if (blob.len < 1) {
		blob.len = strlen(CACERT);
	}
	
	curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 30L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 15L);
	//curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
	curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	#ifdef DEBUG
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
	#endif
	
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(handle, CURLOPT_CAINFO, NULL);
	curl_easy_setopt(handle, CURLOPT_CAPATH, NULL);
	curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &blob);
	
}

CURL* curl_easy_global(void) {
	
	curl_init_globals();
	
	if (curl != NULL) {
		return curl;
	}
	
	curl = curl_easy_new();
	
	if (curl == NULL) {
		return NULL;
	}
	
	return curl;
	
}

CURL* curl_easy_new(void) {
	
	curl_init_globals();
	
	CURL* handle = curl_easy_init();
	
	if (handle == NULL) {
		return NULL;
	}
	
	curl_set_options(handle);
	
	return handle;
	
}

CURLM* curl_multi_global(void) {
	
	curl_init_globals();
	
	if (curl_multi != NULL) {
		return curl_multi;
	}
	
	curl_multi = curl_multi_init();
	
	if (curl_multi == NULL) {
		return NULL;
	}
	
	curl_multi_setopt(curl_multi, CURLMOPT_MAX_HOST_CONNECTIONS, 30L);
	curl_multi_setopt(curl_multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 30L);
	
	return curl_multi;
	
}
	