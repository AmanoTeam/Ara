#include <stdlib.h>
#include <string.h>

#include "symbols.h"
#include "embed_stream.h"
#include "errors.h"

static const char VIMEO_URL_PATTERN[] = "//player.vimeo.com/video";
static const char YOUTUBE_URL_PATTERN[] = "//www.youtube.com/embed";

static const char URL_PREFIX[] = "https:";

static int alloc_url(const char* const start, const size_t size, char** dst) {
	
	const size_t buffer_size = strlen(URL_PREFIX) + size + 1;
	char* uri = malloc(buffer_size);
	
	if (uri == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(uri, URL_PREFIX);
	
	memcpy(uri + strlen(URL_PREFIX), start, size);
	uri[buffer_size - 1] = '\0';
	
	*dst = uri;
	
	return UERR_SUCCESS;
	
}

int embed_stream_find(const char* const content, struct EmbedStream* const info) {
	
	const char* start = strstr(content, VIMEO_URL_PATTERN);
	
	if (start != NULL) {
		const char* const end = strstr(start, QUOTATION_MARK);
		
		if (end == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		const int code = alloc_url(start, (size_t) (end - start), &info->uri);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		info->type = EMBED_STREAM_VIMEO;
		
		return UERR_SUCCESS;
	}
	
	start = strstr(content, YOUTUBE_URL_PATTERN);
	
	if (start != NULL) {
		const char* const end = strstr(start, QUOTATION_MARK);
		
		if (end == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		const int code = alloc_url(start, (size_t) (end - start), &info->uri);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		info->type = EMBED_STREAM_YOUTUBE;
		
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
