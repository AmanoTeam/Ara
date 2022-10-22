#include <stdlib.h>

#include <bearssl.h>

#include "utils.h"

char* sha256_digest(const char* const s, char* dst) {
	
	br_sha256_context context = {0};
	br_sha256_init(&context);
	br_sha256_update(&context, s, strlen(s));
	
	char sha256[br_sha256_SIZE];
	br_sha256_out(&context, sha256);
	
	size_t dst_offset = 0;
	
	for (size_t index = 0; index < sizeof(sha256); index++) {
		const char ch = sha256[index];
		
		dst[dst_offset++] = to_hex((ch & 0xF0) >> 4);
		dst[dst_offset++] = to_hex((ch & 0x0F) >> 0);
	}
	
	dst[dst_offset] = '\0';
	
	return dst;
	
}
