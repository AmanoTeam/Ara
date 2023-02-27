#include <stdlib.h>
#include <string.h>

#include <jansson.h>
#include <curl/curl.h>

#include "cleanup.h"
#include "errors.h"
#include "symbols.h"
#include "callbacks.h"
#include "stringu.h"
#include "query.h"
#include "types.h"
#include "youtube.h"
#include "curl.h"
#include "curl_cleanup.h"
#include "query_cleanup.h"
#include "buffer_cleanup.h"
#include "cleanup.h"

static const char YOUTUBE_URL_PATTERN[] = "https://www.youtube.com/embed";

static const char YOUTUBE_PLAYER_ENDPOINT[] = "https://www.youtube.com/youtubei/v1/player";
static const char YOUTUBE_PLAYER_KEY[] = "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8";

#define YOUTUBE_CLIENT_VERSION "17.43.36"

static const char YOUTUBE_CLIENT_USER_AGENT[] = "com.google.android.youtube/" YOUTUBE_CLIENT_VERSION " (Linux; U; Android 13)";

static const char* const YOUTUBE_PLAYER_HEADERS[][2] = {
	{"X-Youtube-Client-Name", "3"},
	{"X-Youtube-Client-Version", YOUTUBE_CLIENT_VERSION},
	{"Origin", "https://www.youtube.com"},
	{"Content-Type", "application/json"},
	{"User-Agent", YOUTUBE_CLIENT_USER_AGENT}
};

int youtube_matches(const char* const url) {
	
	const int matches = memcmp(url, YOUTUBE_URL_PATTERN, strlen(YOUTUBE_URL_PATTERN)) == 0;
	return matches;
	
}

int youtube_parse(
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
	
	char* path __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_PATH, &path, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	const char* const start = basename(path);
	
	char video_id[strlen(start) + 1];
	strcpy(video_id, start);
	
	struct curl_slist* list __curl_slist_free_all__ = NULL;
	
	for (size_t index = 0; index < sizeof(YOUTUBE_PLAYER_HEADERS) / sizeof(*YOUTUBE_PLAYER_HEADERS); index++) {
		const char* const* const header = YOUTUBE_PLAYER_HEADERS[index];
		
		const char* const key = header[0];
		const char* const value = header[1];
		
		char item[strlen(key) + strlen(HTTP_HEADER_SEPARATOR) + strlen(value) + 1];
		strcpy(item, key);
		strcat(item, HTTP_HEADER_SEPARATOR);
		strcat(item, value);
		
		struct curl_slist* const tmp = curl_slist_append(list, item);
		
		if (tmp == NULL) {
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	
	struct Query query __query_free__ = {0};
	
	add_parameter(&query, "token", YOUTUBE_PLAYER_KEY);
	add_parameter(&query, "prettyPrint", "false");
	
	char* get_fields __free__ = NULL;
	const int code = query_stringify(query, &get_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, YOUTUBE_PLAYER_ENDPOINT, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_QUERY, get_fields, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	char* uri __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_URL, &uri, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_URL, uri);
	
	json_auto_t* subtree = json_object();
	
	json_t* const client = json_object();
	
	json_object_set_new(client, "clientName", json_string("ANDROID"));
	json_object_set_new(client, "clientVersion", json_string(YOUTUBE_CLIENT_VERSION));
	json_object_set_new(client, "androidSdkVersion", json_integer(33));
	json_object_set_new(client, "userAgent", json_string(YOUTUBE_CLIENT_USER_AGENT));
	json_object_set_new(client, "hl", json_string("en"));
	json_object_set_new(client, "timeZone", json_string("UTC"));
	json_object_set_new(client, "utcOffsetMinutes", json_integer(0));
	
	json_t* const context = json_object();
	json_object_set_new(context, "client", client);
	json_object_set_new(subtree, "context", context);
	
	json_t* const content_playback_context = json_object();
	json_object_set_new(content_playback_context, "html5Preference", json_string("HTML5_PREF_WANTS"));
	
	json_t* const playback_context = json_object();
	json_object_set_new(playback_context, "contentPlaybackContext", content_playback_context);
	
	json_object_set_new(subtree, "playbackContext", playback_context);
	
	json_object_set_new(subtree, "videoId", json_string(video_id));
	
	char* post_fields __free__ = json_dumps(subtree, JSON_COMPACT);
	
	if (post_fields == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "playabilityStatus");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "status");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const status = json_string_value(obj);
	
	if (strcmp(status, "OK") != 0) {
		return UERR_NO_STREAMS_AVAILABLE;
	}
	
	obj = json_object_get(tree, "streamingData");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "formats");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	json_int_t last_itag = 0;
	const char* stream_uri = NULL;
	
	size_t index = 0;
	const json_t* item = NULL;
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "itag");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_integer(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_int_t itag = json_integer_value(obj);
		
		if (last_itag < itag) {
			const json_t* obj = json_object_get(item, "url");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const url = json_string_value(obj);
			
			last_itag = itag;
			stream_uri = url;
		}
	}
	
	if (stream_uri == NULL) {
		return UERR_NO_STREAMS_AVAILABLE;
	}
	
	obj = json_object_get(tree, "videoDetails");
	
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
	
	media->video.id = malloc(strlen(video_id) + 1);
	media->video.filename = malloc(strlen(title) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1);
	media->video.short_filename = malloc(strlen(video_id) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1);
	
	if (media->video.filename == NULL || media->video.short_filename == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->video.id, video_id);
	
	strcpy(media->video.filename, title);
	strcat(media->video.filename, DOT);
	strcat(media->video.filename, MP4_FILE_EXTENSION);
	
	strcpy(media->video.short_filename, video_id);
	strcat(media->video.short_filename, DOT);
	strcat(media->video.short_filename, MP4_FILE_EXTENSION);
	
	normalize_filename(media->video.filename);
	
	media->video.url = malloc(strlen(stream_uri) + 1);
	
	if (media->video.url == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->video.url, stream_uri);
	
	media->type = MEDIA_SINGLE;
	
	return UERR_SUCCESS;
	
}
