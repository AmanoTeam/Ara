#include <stdlib.h>

#include "types.h"

size_t curl_write_cb(char *chunk, size_t size, size_t nmemb, struct String* string) {
	
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