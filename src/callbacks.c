#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "types.h"
#include "callbacks.h"
#include "fstream.h"

#if defined(WIN32) && defined(UNICODE)
	#include "wio.h"
#endif

size_t curl_write_string_cb(char *chunk, size_t size, size_t nmemb, struct String* string) {
	
	const size_t chunk_size = size * nmemb;
	const size_t slength = string->slength + chunk_size;
	
	string->s = realloc(string->s, slength + 1);
	
	if (string->s == NULL) {
		return 0;
	}
	
	memcpy(string->s + string->slength, chunk, chunk_size);
	
	string->s[slength] = '\0';
	string->slength = slength;
	
	return chunk_size;
	
}

size_t curl_progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	
	(void) (clientp);
	(void) (ultotal);
	(void) (ulnow);
	
	if (dltotal < 1) {
		return 0;
	}
	
	printf("\r+ Atualmente em progresso: %" CURL_FORMAT_CURL_OFF_T "%% / 100%%\r", ((dlnow * 100) / dltotal));
	
	fflush(stdout);
	
	return 0;
	
}

size_t curl_write_file_cb(char *chunk, size_t size, size_t nmemb, struct FStream* stream) {
	
	const size_t chunk_size = size * nmemb;
	
	if (!fstream_write(stream, chunk, chunk_size)) {
		return 0;
	}
	
	return chunk_size;
	
}