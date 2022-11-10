#include <stdlib.h>
#include <string.h>

#include <jansson.h>
#include <curl/curl.h>

#include "cleanup.h"
#include "errors.h"
#include "symbols.h"
#include "callbacks.h"
#include "utils.h"
#include "types.h"
#include "query.h"
#include "string.h"
#include "youtube.h"

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

static const char MP4_FILE_EXTENSION[] = "mp4";

int youtube_parse(CURL* curl, const char* const uri, struct Media* media) {
	
	const char* const start = basename(uri);
	
	char video_id[strlen(start) + 1];
	strcpy(video_id, start);
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(YOUTUBE_PLAYER_HEADERS) / sizeof(*YOUTUBE_PLAYER_HEADERS); index++) {
		const char** const header = (const char**) YOUTUBE_PLAYER_HEADERS[index];
		const char* const key = header[0];
		const char* const value = header[1];
		
		char item[strlen(key) + strlen(HTTP_HEADER_SEPARATOR) + strlen(value) + 1];
		strcpy(item, key);
		strcat(item, HTTP_HEADER_SEPARATOR);
		strcat(item, value);
		
		struct curl_slist* tmp = curl_slist_append(list, item);
		
		if (tmp == NULL) {
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	
	struct Query uquery __attribute__((__cleanup__(query_free))) = {0};
	
	add_parameter(&uquery, "token", YOUTUBE_PLAYER_KEY);
	add_parameter(&uquery, "prettyPrint", "false");
	
	char* query __attribute__((__cleanup__(charpp_free))) = NULL;
	const int code = query_stringify(uquery, &query);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU* cu __attribute__((__cleanup__(curlupp_free))) = curl_url();
	curl_url_set(cu, CURLUPART_URL, YOUTUBE_PLAYER_ENDPOINT, 0);
	curl_url_set(cu, CURLUPART_QUERY, query, 0);
	
	char* url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
	curl_url_get(cu, CURLUPART_URL, &url, 0);
	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	json_auto_t* subtree = json_object();
	
	json_auto_t* client = json_object();
	
	json_object_set_new(client, "clientName", json_string("ANDROID"));
	json_object_set_new(client, "clientVersion", json_string(YOUTUBE_CLIENT_VERSION));
	json_object_set_new(client, "androidSdkVersion", json_integer(33));
	json_object_set_new(client, "userAgent", json_string(YOUTUBE_CLIENT_USER_AGENT));
	json_object_set_new(client, "hl", json_string("en"));
	json_object_set_new(client, "timeZone", json_string("UTC"));
	json_object_set_new(client, "utcOffsetMinutes", json_integer(0));
	
	json_auto_t* context = json_object();
	json_object_set_new(context, "client", client);
	json_object_set_new(subtree, "context", context);
	
	json_auto_t* content_playback_context = json_object();
	json_object_set_new(content_playback_context, "html5Preference", json_string("HTML5_PREF_WANTS"));
	
	json_auto_t* playback_context = json_object();
	json_object_set_new(playback_context, "contentPlaybackContext", content_playback_context);
	
	json_object_set_new(subtree, "playbackContext", playback_context);
	
	json_object_set_new(subtree, "videoId", json_string(video_id));
	
	char* post_fields __attribute__((__cleanup__(charpp_free))) = json_dumps(subtree, JSON_COMPACT);
	
	if (post_fields == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_fields);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "streamingData");
	
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
	
	int last_itag = 0;
	const char* stream_uri = NULL;
	
	size_t index = 0;
	json_t *item = NULL;
	
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
		
		const int itag = json_integer_value(obj);
		
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
	
	media->filename = malloc(strlen(title) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1);
	
	if (media->filename == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->filename, title);
	strcat(media->filename, DOT);
	strcat(media->filename, MP4_FILE_EXTENSION);
	
	normalize_filename(media->filename);
	
	media->url = malloc(strlen(stream_uri) + 1);
	
	if (media->url == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(media->url, stream_uri);
	
	media->type = MEDIA_SINGLE;
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	
	return UERR_SUCCESS;
	
}
