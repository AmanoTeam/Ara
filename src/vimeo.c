#include <stdlib.h>
#include <string.h>

#include <jansson.h>
#include <curl/curl.h>

#include "errors.h"
#include "symbols.h"
#include "callbacks.h"
#include "m3u8.h"
#include "cleanup.h"
#include "stringu.h"
#include "types.h"
#include "vimeo.h"
#include "buffer.h"
#include "buffer_cleanup.h"
#include "curl.h"
#include "curl_cleanup.h"

static const char VIMEO_URL_PATTERN[] = "https://player.vimeo.com/video";

static const char JSON_TREE_PATTERN[] = "window.playerConfig = ";

int vimeo_matches(const char* const url) {
	
	const int matches = memcmp(url, VIMEO_URL_PATTERN, strlen(VIMEO_URL_PATTERN)) == 0;
	return matches;
	
}

int vimeo_parse(
	const char* const url,
	const struct Resource* const resource,
	const struct Page* const page,
	struct Media* const media,
	const char* const referer
) {
	
	(void) resource;
	(void) page;
	
	CURL* const curl_easy = get_global_curl_easy();
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	curl_easy_setopt(curl_easy, CURLOPT_REFERER, referer);
	
	const CURLcode code = curl_easy_perform_retry(curl_easy);
	
	if (code == CURLE_HTTP_RETURNED_ERROR) {
		long status_code = 0;
		curl_easy_getinfo(curl_easy, CURLINFO_RESPONSE_CODE, &status_code);
		
		if (status_code == 404) {
			return UERR_NO_STREAMS_AVAILABLE;
		}
		
		return UERR_CURL_FAILURE;
	} else if (code != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_REFERER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	const char* start = strstr(string.s, JSON_TREE_PATTERN);
	
	if (start == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	start += strlen(JSON_TREE_PATTERN);
	
	const char* const patterns[] = {"}; ", "}\n"};
	
	const char* end = NULL;
	
	for (size_t index = 0; index < sizeof(patterns) / sizeof(*patterns); index++) {
		const char* const pattern = patterns[index];
		
		if ((end = strstr(start, pattern)) != NULL) {
			break;
		}
	}
	
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
	
	const json_t* video = json_object_get(tree, "video");
	
	if (video == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(video)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* obj = json_object_get(video, "title");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const title = json_string_value(obj);
	
	obj = json_object_get(video, "id");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_integer(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t id = json_integer_value(obj);
	
	char sid[intlen(id) + 1];
	snprintf(sid, sizeof(sid), "%llu", id);
	
	obj = json_object_get(tree, "request");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* const files = json_object_get(obj, "files");
	
	if (files == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(files)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(files, "progressive");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const size_t total_items = json_array_size(obj);
	
	if (total_items > 0) {
		json_int_t last_width = 0;
		const char* stream_uri = NULL;
		
		size_t index = 0;
		const json_t* item = NULL;
		
		json_array_foreach(obj, index, item) {
			if (!json_is_object(item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* const obj = json_object_get(item, "width");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_integer(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_int_t width = json_integer_value(obj);
			
			if (last_width < width) {
				const json_t* const obj = json_object_get(item, "url");
				
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
		
		const char* const file_extension = get_file_extension(stream_uri);
		
		if (file_extension == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		media->video.id = malloc(strlen(sid) + 1);
		media->video.filename = malloc(strlen(title) + strlen(DOT) + strlen(file_extension) + 1);
		media->video.short_filename = malloc(strlen(sid) + strlen(DOT) + strlen(file_extension) + 1);
		
		if (media->video.id == NULL || media->video.filename == NULL || media->video.short_filename == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(media->video.filename, title);
		strcat(media->video.filename, DOT);
		strcat(media->video.filename, file_extension);
		
		strcpy(media->video.short_filename, sid);
		strcat(media->video.short_filename, DOT);
		strcat(media->video.short_filename, file_extension);
		
		normalize_filename(media->video.filename);
		
		media->video.url = malloc(strlen(stream_uri) + 1);
		
		if (media->video.url == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(media->video.url, stream_uri);
		
		media->type = MEDIA_SINGLE;
	} else {
		const json_t* obj = json_object_get(files, "hls");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* const subobj = json_object_get(obj, "default_cdn");
		
		if (subobj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(subobj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const default_cdn = json_string_value(subobj);
		
		obj = json_object_get(obj, "cdns");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		obj = json_object_get(obj, default_cdn);
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		obj = json_object_get(obj, "url");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const url = json_string_value(obj);
		
		buffer_t string __buffer_free__ = {0};
		
		curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
		curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
		curl_easy_setopt(curl_easy, CURLOPT_URL, url);
		
		if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
			
		struct Tags tags = {0};
		
		if (m3u8_parse(&tags, string.s) != UERR_SUCCESS) {
			return UERR_M3U8_PARSE_FAILURE;
		}
		
		int last_width = 0;
		
		const char* video_stream = NULL;
		const char* audio_stream = NULL;
		
		for (size_t index = 0; index < tags.offset; index++) {
			const struct Tag* const tag = &tags.items[index];
			
			switch (tag->type) {
				case EXT_X_STREAM_INF: {
					const struct Attribute* const attribute = attributes_get(&tag->attributes, "RESOLUTION");
					
					if (attribute == NULL) {
						return UERR_M3U8_MISSING_REQUIRED_ATTRIBUTE;
					}
					
					const char* const start = attribute->value;
					const char* const end = strstr(start, "x");
					
					const size_t size = (size_t) (end - start);
					
					char value[size + 1];
					memcpy(value, start, size);
					value[size] = '\0';
					
					const int width = atoi(value);
					
					if (last_width < width) {
						last_width = width;
						video_stream = tag->uri;
					}
					
					break;
				}
				case EXT_X_MEDIA: {
					if (audio_stream != NULL) {
						break;
					}
					
					const struct Attribute* attribute = attributes_get(&tag->attributes, "TYPE");
					
					if (attribute == NULL) {
						return UERR_M3U8_MISSING_REQUIRED_ATTRIBUTE;
					}
					
					if (strcmp(attribute->value, "AUDIO") != 0) {
						break;
					}
					
					 attribute = attributes_get(&tag->attributes, "URI");
					
					if (attribute == NULL) {
						return UERR_M3U8_MISSING_REQUIRED_ATTRIBUTE;
					}
					
					audio_stream = attribute->value;
					
					break;
				}
				default: {
					break;
				}
			}
		}
		
		if (video_stream == NULL || audio_stream == NULL) {
			return UERR_NO_STREAMS_AVAILABLE;
		}
		
		CURLU* cu __curl_url_cleanup__ = curl_url();
		
		if (cu == NULL) {
			return UERR_CURLU_FAILURE;
		}
		
		if (curl_url_set(cu, CURLUPART_URL, url, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		if (curl_url_set(cu, CURLUPART_URL, video_stream, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		if (curl_url_get(cu, CURLUPART_URL, &media->video.url, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		if (curl_url_set(cu, CURLUPART_URL, url, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		if (curl_url_set(cu, CURLUPART_URL, audio_stream, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		if (curl_url_get(cu, CURLUPART_URL, &media->audio.url, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		media->audio.id = malloc(strlen(sid) + 1);
		media->audio.filename = malloc(strlen(title) + strlen(DOT) + strlen(AAC_FILE_EXTENSION) + 1);
		media->audio.short_filename = malloc(strlen(sid) + strlen(DOT) + strlen(AAC_FILE_EXTENSION) + 1);
		
		if (media->audio.id == NULL || media->audio.filename == NULL || media->audio.short_filename == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(media->audio.id, sid);
		
		strcpy(media->audio.filename, title);
		strcat(media->audio.filename, DOT);
		strcat(media->audio.filename, AAC_FILE_EXTENSION);
		
		strcpy(media->audio.short_filename, sid);
		strcat(media->audio.short_filename, DOT);
		strcat(media->audio.short_filename, AAC_FILE_EXTENSION);
		
		normalize_filename(media->audio.filename);
		
		media->video.id = malloc(strlen(sid) + 1);
		media->video.filename = malloc(strlen(title) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1);
		media->video.short_filename = malloc(strlen(sid) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1);
		
		if (media->video.id == NULL || media->video.filename == NULL || media->video.short_filename == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(media->video.id, sid);
		
		strcpy(media->video.filename, title);
		strcat(media->video.filename, DOT);
		strcat(media->video.filename, MP4_FILE_EXTENSION);
		
		strcpy(media->video.short_filename, sid);
		strcat(media->video.short_filename, DOT);
		strcat(media->video.short_filename, MP4_FILE_EXTENSION);
		
		normalize_filename(media->video.filename);
		
		media->type = MEDIA_M3U8;
	}
	
	return UERR_SUCCESS;
	
}
