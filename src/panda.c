#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "errors.h"
#include "symbols.h"
#include "callbacks.h"
#include "m3u8.h"
#include "cleanup.h"
#include "stringu.h"
#include "types.h"
#include "panda.h"
#include "buffer.h"
#include "buffer_cleanup.h"
#include "curl.h"
#include "curl_cleanup.h"

static const char PANDA_URL_PATTERN_START[] = "https://player-vz-";
static const char PANDA_URL_PATTERN_END[] = ".tv.pandavideo.com.br/embed/";

static const size_t PANDA_SUBDOMAIN_CODE_SIZE = 12;

static const char PLAYLIST_FILENAME[] = "playlist.m3u8";
static const char V_QUERY_PATTERN[] = "v=";

int panda_matches(const char* const url) {
	
	const int matches = (memcmp(url, PANDA_URL_PATTERN_START, strlen(PANDA_URL_PATTERN_START)) == 0
		&& (strlen(PANDA_URL_PATTERN_START) + PANDA_SUBDOMAIN_CODE_SIZE) < strlen(url)
		&& strstr(url + PANDA_SUBDOMAIN_CODE_SIZE, PANDA_URL_PATTERN_END) != NULL);
	return matches;
	
}

int panda_parse(
	const char* const url,
	const struct Resource* const resource,
	const struct Page* const page,
	struct Media* const media,
	const char* const referer
) {
	
	(void) resource;
	(void) page;
	(void) referer;
	
	CURL* const curl_easy = get_global_curl_easy();
	
	CURLU* cu __curl_url_cleanup__ = curl_url();
	
	if (cu == NULL) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	char* query __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_QUERY, &query, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (query == NULL || *query == '\0') {
		return UERR_STRSTR_FAILURE;
	}
	
	const char* start = strstr(query, V_QUERY_PATTERN);
	
	if (start == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	start += strlen(V_QUERY_PATTERN);
	
	const char* end = strstr(query, AND);
	
	if (end == NULL) {
		end = strchr(start, '\0');
	}
	
	const size_t size = (size_t) (end - start);
	
	char media_code[size + 1];
	memcpy(media_code, start, size);
	media_code[size] = '\0';
	
	char* host __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_HOST, &host, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	const char* const pattern = "player-";
	const char* host_start = strstr(host, pattern);
	
	if (host_start == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	const char* const prefix = "b-";
	
	char playlist_url[strlen(HTTPS_SCHEME) + strlen(prefix) +  strlen(host + strlen(pattern)) + strlen(SLASH) + strlen(media_code) + strlen(SLASH) + strlen(PLAYLIST_FILENAME) + 1];
	strcpy(playlist_url, HTTPS_SCHEME);
	strcat(playlist_url, prefix);
	strcat(playlist_url, host + strlen(pattern));
	strcat(playlist_url, SLASH);
	strcat(playlist_url, media_code);
	strcat(playlist_url, SLASH);
	strcat(playlist_url, PLAYLIST_FILENAME);
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, playlist_url);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	struct Tags tags = {0};
	
	if (m3u8_parse(&tags, string.s) != UERR_SUCCESS) {
		return UERR_M3U8_PARSE_FAILURE;
	}
	
	int last_width = 0;
	const char* last_uri = NULL;
	
	for (size_t index = 0; index < tags.offset; index++) {
		struct Tag* tag = &tags.items[index];
		
		if (tag->type != EXT_X_STREAM_INF) {
			continue;
		}
		
		const struct Attribute* const attribute = attributes_get(&tag->attributes, "RESOLUTION");
		
		const char* const start = attribute->value;
		const char* const end = strstr(start, "x");
		
		const size_t size = (size_t) (end - start);
		
		char value[size + 1];
		memcpy(value, start, size);
		value[size] = '\0';
		
		const int width = atoi(value);
		
		if (last_width < width) {
			last_width = width;
			last_uri = tag->uri;
		}
	}
	
	if (curl_url_set(cu, CURLUPART_URL, playlist_url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, last_uri, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_get(cu, CURLUPART_URL, &media->video.url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	const int hash = hashs(media_code);
	
	char id[intlen(hash) + 1];
	snprintf(id, sizeof(id), "%i", hash);
	
	media->type = MEDIA_M3U8;
	media->video.id = malloc(strlen(id) + 1);
	media->video.filename = malloc(strlen(id) + strlen(DOT) + strlen(TS_FILE_EXTENSION) + 1);
	media->video.short_filename = malloc(strlen(id) + strlen(DOT) + strlen(TS_FILE_EXTENSION) + 1);
	
	if (media->video.id == NULL || media->video.filename == NULL || media->video.short_filename == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->video.id, id);
	
	strcpy(media->video.filename, id);
	strcat(media->video.filename, DOT);
	strcat(media->video.filename, TS_FILE_EXTENSION);
	
	strcpy(media->video.short_filename, media->video.filename);
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}
