#include <stdlib.h>
#include <string.h>

#include "symbols.h"
#include "embed_stream.h"
#include "errors.h"

static const char VIMEO_URL_PATTERN[] = "https://player.vimeo.com";

int embed_stream_find(const char* const content, struct EmbedStream* const info) {
	
	const char* const start = strstr(content, VIMEO_URL_PATTERN);
	
	if (start != NULL) {
		const char* const end = strstr(start, QUOTATION_MARK);
		
		if (end == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		const size_t size = (size_t) (end - start);
		
		info->uri = malloc(size + 1);
		
		if (info->uri == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		memcpy(info->uri, start, size);
		info->uri[size] = '\0';
		
		info->type = EMBED_STREAM_VIMEO;
		
		return UERR_SUCCESS;
	}
	
	return UERR_NO_STREAMS_AVAILABLE;
	
}

void embed_stream_free(struct EmbedStream* const obj) {
	
	if (obj->uri != NULL) {
		free(obj->uri);
		obj->uri = NULL;
	}
	
	obj->type = 0;
	
}
