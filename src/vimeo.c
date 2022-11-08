#include <stdlib.h>
#include <string.h>

#include <jansson.h>

#include "errors.h"
#include "symbols.h"
#include "utils.h"
#include "types.h"
#include "vimeo.h"

static const char PATTERN[] = "window.playerConfig = ";

int vimeo_parse(const char* const content, struct Media* media) {
	
	const char* start = strstr(content, PATTERN);
	
	if (start == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	start += strlen(PATTERN);
	
	const char* end = strstr(start, "}; ");
	
	if (end == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	end += 1;
	
	const size_t size = (size_t) (end - start);
	
	char text[size + 1];
	memcpy(text, start, size);
	text[size] = '\0';
	
	json_auto_t* tree = json_loads(text, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "request");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "files");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "progressive");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	int last_width = 0;
	const char* stream_uri = NULL;
	
	size_t index = 0;
	json_t *item = NULL;
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "width");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_integer(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const int width = json_integer_value(obj);
		
		if (last_width < width) {
			const json_t* obj = json_object_get(item, "url");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const url = json_string_value(obj);
			
			last_width = width;
			stream_uri = url;
		}
	}
	
	if (stream_uri == NULL) {
		return UERR_NO_STREAMS_AVAILABLE;
	}
	
	obj = json_object_get(tree, "video");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "title");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const title = json_string_value(obj);
	const char* const file_extension = get_file_extension(stream_uri);
	
	if (file_extension == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	media->filename = malloc(strlen(title) + strlen(DOT) + strlen(file_extension) + 1);
	
	if (media->filename == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->filename, title);
	strcat(media->filename, DOT);
	strcat(media->filename, file_extension);
	
	media->url = malloc(strlen(stream_uri) + 1);
	
	if (media->url == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->url, stream_uri);
	
	media->type = MEDIA_SINGLE;
	
	return UERR_SUCCESS;
	
}