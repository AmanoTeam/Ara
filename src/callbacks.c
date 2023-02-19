#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "callbacks.h"
#include "types.h"
#include "fstream.h"

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

size_t curl_write_string_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	
	struct String* string = (struct String*) userdata;
	
	const size_t chunk_size = size * nmemb;
	const size_t slength = string->slength + chunk_size;
	
	string->s = realloc(string->s, slength + 1);
	
	if (string->s == NULL) {
		return 0;
	}
	
	memcpy(string->s + string->slength, ptr, chunk_size);
	
	string->s[slength] = '\0';
	string->slength = slength;
	
	return chunk_size;
	
}

size_t curl_progress_cb(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	
	(void) (clientp);
	(void) (ultotal);
	(void) (ulnow);
	
	const curl_off_t progress = dltotal < 1 ? 0 : (dlnow * 100) / dltotal;
	
	printf("\r+ Atualmente em progresso: %" CURL_FORMAT_CURL_OFF_T "%% / 100%%\r", progress);
	
	fflush(stdout);
	
	return 0;
	
}

size_t curl_write_file_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	
	struct FStream* const stream = (struct FStream*) userdata;
	
	const size_t chunk_size = size * nmemb;
	
	if (fstream_write(stream, ptr, chunk_size) == -1) {
		return 0;
	}
	
	return chunk_size;
	
}

size_t curl_discard_body_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	
	(void) ptr;
	(void) userdata;
	
	return size * nmemb;
	
}

size_t json_load_cb(void* buffer, size_t buflen, void* data) {
	
	const ssize_t size = fstream_read((struct FStream*) data, (char*) buffer, buflen);
	return (size_t) size;
	
}

int json_dump_cb(const char *buffer, size_t size, void* data) {
	
	const int status = fstream_write((struct FStream*) data, buffer, size);
	
	return status;
	
}
