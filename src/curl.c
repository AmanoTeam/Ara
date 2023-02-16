#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <synchapi.h>
#else
	#include <unistd.h>
#endif

#include <curl/curl.h>

#include "curl.h"
#include "filesystem.h"
#include "stringu.h"
#include "symbols.h"
#include "fstream.h"
#include "errors.h"

#ifndef SPARKLEC_DISABLE_CERTIFICATE_VALIDATION
	static const char CA_CERT_FILENAME[] = 
		PATH_SEPARATOR
		"etc"
		PATH_SEPARATOR
		"tls"
		PATH_SEPARATOR
		"cert.pem";
#endif

static const char HTTP_DEFAULT_USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/109.0.5414.119 Safari/537.36";
static const long HTTP_MAX_CONCURRENT_CONNECTIONS = 30L;
static const size_t HTTP_MAX_RETRIES = 10;

static char CURL_ERROR_MESSAGE[CURL_ERROR_SIZE] = {'\0'};

static int GLOBALS_INITIALIZED = 0;

static CURL* curl_easy_global = NULL;
static CURLM* curl_multi_global = NULL;
static struct curl_blob curl_blob_global = {0};

static void globals_destroy(void) {
	
	curl_multi_cleanup(curl_multi_global);
	curl_multi_global = NULL;
	
	curl_easy_cleanup(curl_easy_global);
	curl_easy_global = NULL;
	
	if (curl_blob_global.data != NULL) {
		free(curl_blob_global.data);
		
		curl_blob_global.data = NULL;
		curl_blob_global.len = 0;
	}
	
}

static int globals_initialize(void) {
	
	if (GLOBALS_INITIALIZED) {
		return UERR_SUCCESS;
	}
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	atexit(globals_destroy);
	
	#ifndef SPARKLEC_DISABLE_CERTIFICATE_VALIDATION
		char app_filename[PATH_MAX];
		get_app_filename(app_filename);
		
		char app_root_directory[PATH_MAX];
		get_parent_directory(app_filename, app_root_directory, 2);
		
		char ca_bundle[strlen(app_root_directory) + strlen(CA_CERT_FILENAME) + 1];
		strcpy(ca_bundle, app_root_directory);
		strcat(ca_bundle, CA_CERT_FILENAME);
		
		const long long file_size = get_file_size(ca_bundle);
		
		struct FStream* const stream = fstream_open(ca_bundle, FSTREAM_READ);
		
		if (stream == NULL) {
			return UERR_FSTREAM_FAILURE;
		}
		
		curl_blob_global.data = malloc((size_t) file_size);
		
		if (curl_blob_global.data == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		const ssize_t rsize = fstream_read(stream, curl_blob_global.data, (size_t) file_size);
		
		fstream_close(stream);
		
		if (rsize != (ssize_t) file_size) {
			return UERR_FSTREAM_FAILURE;
		}
		
		curl_blob_global.len = (size_t) rsize;
	#endif
	
	GLOBALS_INITIALIZED = 1;
	
	return UERR_SUCCESS;
	
}

static int curl_set_options(CURL* handle) {
	
	curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 30L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 15L);
	curl_easy_setopt(handle, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
	curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	curl_easy_setopt(handle, CURLOPT_CAINFO, NULL);
	curl_easy_setopt(handle, CURLOPT_CAPATH, NULL);
	curl_easy_setopt(handle, CURLOPT_DNS_CACHE_TIMEOUT, -1L);
	curl_easy_setopt(handle, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(handle, CURLOPT_DNS_SHUFFLE_ADDRESSES, 1L);
	
	#ifdef SPARKLEC_DISABLE_CERTIFICATE_VALIDATION
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
	#else
		if (curl_blob_global.data == NULL) {
			curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
		} else {
			curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &curl_blob_global);
		}
	#endif
	
	return UERR_SUCCESS;
	
}

CURL* get_global_curl_easy(void) {
	
	if (globals_initialize() != UERR_SUCCESS) {
		return NULL;
	}
	
	if (curl_easy_global != NULL) {
		return curl_easy_global;
	}
	
	curl_easy_global = curl_easy_new();
	
	if (curl_easy_global == NULL) {
		return NULL;
	}
	
	curl_easy_setopt(curl_easy_global, CURLOPT_ERRORBUFFER, CURL_ERROR_MESSAGE);
	
	return curl_easy_global;
	
}

CURL* curl_easy_new(void) {
	
	if (globals_initialize() != UERR_SUCCESS) {
		return NULL;
	}
	
	CURL* handle = curl_easy_init();
	
	if (handle == NULL) {
		return NULL;
	}
	
	curl_set_options(handle);
	
	return handle;
	
}

CURLM* get_global_curl_multi(void) {
	
	if (globals_initialize() != UERR_SUCCESS) {
		return NULL;
	}
	
	if (curl_multi_global != NULL) {
		return curl_multi_global;
	}
	
	curl_multi_global = curl_multi_init();
	
	if (curl_multi_global == NULL) {
		return NULL;
	}
	
	curl_multi_setopt(curl_multi_global, CURLMOPT_MAX_HOST_CONNECTIONS, HTTP_MAX_CONCURRENT_CONNECTIONS);
	curl_multi_setopt(curl_multi_global, CURLMOPT_MAX_TOTAL_CONNECTIONS, HTTP_MAX_CONCURRENT_CONNECTIONS);
	
	return curl_multi_global;
	
}

const char* get_global_curl_error(void) {
	
	return CURL_ERROR_MESSAGE;
	
}

CURLcode curl_easy_perform_retry(CURL* const curl) {
	
	size_t retry_after = 1;
	size_t retries = 0;
	
	while (1) {
		const CURLcode code = curl_easy_perform(curl);
		
		switch (code) {
			case CURLE_HTTP_RETURNED_ERROR: {
				long status_code = 0;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
				
				if (!(status_code == 408 || status_code == 429 || status_code == 500 || status_code == 502 || status_code == 503 || status_code == 504)) {
					return code;
				}
				
				break;
			}
			case CURLE_OPERATION_TIMEDOUT:
				break;
			default:
				return code;
		}
		
		retries++;
		
		if (retries > HTTP_MAX_RETRIES) {
			return code;
		}
		
		retry_after += retry_after;
		
		#ifdef _WIN32
			Sleep((DWORD) (retry_after * 1000));
		#else
			sleep(retry_after);
		#endif
	}
	
}