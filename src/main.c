#include <stdlib.h>
#include <string.h>

#define A "SparkleC"

#ifdef WIN32
	#include <stdio.h>
	#include <fcntl.h>
	#include <io.h>
	#include <locale.h>
	
	#ifdef UNICODE
		#include <stdarg.h>
	#endif
#else
	#include <sys/resource.h>
#endif

#include <curl/curl.h>
#include <jansson.h>

#include "cleanup.h"
#include "errors.h"
#include "query.h"
#include "callbacks.h"
#include "types.h"
#include "utils.h"
#include "symbols.h"
#include "cacert.h"
#include "m3u8.h"
#include "embed_stream.h"
#include "vimeo.h"
#include "youtube.h"

struct SegmentDownload {
	CURL* handle;
	char* filename;
	FILE* stream;
};

#if defined(WIN32) && defined(UNICODE)
	int __printf(const char* const format, ...) {
		
		va_list list;
		va_start(list, format);
		
		const int size = vsnprintf(NULL, 0, format, list);
		char value[size + 1];
		vsnprintf(value, sizeof(value), format, list);
		
		va_end(list);
		
		int wcsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
		wchar_t wvalue[wcsize];
		MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wcsize);
		
		return wprintf(L"%ls", wvalue);
		
	}
	
	int __fprintf(FILE* const stream, const char* const format, ...) {
		
		va_list list;
		va_start(list, format);
		
		const int size = vsnprintf(NULL, 0, format, list);
		char value[size + 1];
		vsnprintf(value, sizeof(value), format, list);
		
		va_end(list);
		
		int wcsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
		wchar_t wvalue[wcsize];
		MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wcsize);
		
		return fwprintf(stream, L"%ls", wvalue);
		
	}
	
	FILE* __fopen(const char* const filename, const char* const mode) {
		
		int wcsize = 0;
		
		wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
		wchar_t wfilename[wcsize];
		MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, sizeof(wfilename) / sizeof(*wfilename));
		
		wcsize = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
		wchar_t wmode[wcsize];
		MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, sizeof(wmode) / sizeof(*wmode));
		
		return _wfopen(wfilename, wmode);
		
	};
	
	char* __fgets(char* const s, const int n, FILE* const stream) {
		
		wchar_t ws[n];
		
		if (fgetws(ws, sizeof(ws) / sizeof(*ws), stream) == NULL) {
			return NULL;
		}
		
		WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL);
		
		return s;
		
	}
	
	#define printf __printf
	#define fprintf __fprintf
	#define fopen __fopen
	#define fgets __fgets
#endif

static size_t progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	(void) (clientp);
	(void) (ultotal);
	(void) (ulnow);
	
	if (dltotal < 1) {
		return 0;
	}
	
	printf("\r+ Atualmente em progresso: %lli%% / 100%%\r", ((dlnow * 100) / dltotal));
	
	fflush(stdout);
	
	return 0;
	
}

static const char MP4_FILE_EXTENSION[] = "mp4";
static const char TS_FILE_EXTENSION[] = "ts";
static const char KEY_FILE_EXTENSION[] = "key";
static const char HTML_FILE_EXTENSION[] = "html";

static const char LOCAL_PLAYLIST_FILENAME[] = "playlist.m3u8";
static const char LOCAL_ACCOUNTS_FILENAME[] = "accounts.json";

static const char HTTPS_SCHEME[] = "https://";

static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";
static const char HTTP_HEADER_REFERER[] = "Referer";
static const char HTTP_HEADER_TOKEN[] = "Token";
static const char HTTP_HEADER_CLUB[] = "Club";
static const char HTTP_DEFAULT_USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36";
static const char HTTP_AUTHENTICATION_BEARER[] = "Bearer";

static const char HOTMART_CLUB_SUFFIX[] = ".club.hotmart.com";

#define HOTMART_API_CLUB_PREFIX "https://api-club.hotmart.com/hot-club-api/rest/v3"
#define HOTMART_API_SEC_PREFIX "https://api-sec-vlc.hotmart.com"
#define SPARKLEAPP_API_PREFIX "https://api.sparkleapp.com.br"

static const char HOTMART_HOMEPAGE[] = "https://hotmart.com";

static const char HOTMART_NAVIGATION_ENDPOINT[] = 
	HOTMART_API_CLUB_PREFIX
	"/navigation";

static const char HOTMART_MEMBERSHIP_ENDPOINT[] = 
	HOTMART_API_CLUB_PREFIX
	"/membership";

static const char HOTMART_PAGE_ENDPOINT[] = 
	HOTMART_API_CLUB_PREFIX
	"/page";

static const char HOTMART_ATTACHMENT_ENDPOINT[] = 
	HOTMART_API_CLUB_PREFIX
	"/attachment";

static const char HOTMART_TOKEN_ENDPOINT[] = 
	SPARKLEAPP_API_PREFIX
	"/oauth/token";

static const char HOTMART_TOKEN_CHECK_ENDPOINT[] = 
	HOTMART_API_SEC_PREFIX
	"/security/oauth/check_token";

static const char HTML_HEADER_START[] = 
	"<!DOCTYPE html>"
	"<html lang=\"pt-BR\">"
	"<head>"
	"<title>Hotmart content</title>"
	"<meta charset=\"utf-8\">"
	"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
	"<meta http-equiv=\"Referrer-Policy\" content=\"no-referrer\">"
	"</head>"
	"<body>";

static const char HTML_HEADER_END[] = 
	"</body>"
	"</html>";

#define MAX_INPUT_SIZE 1024

static CURL* curl = NULL;

static int authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	char* user __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, username, 0);
	
	if (user == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char* pass __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, password, 0);
	
	if (pass == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	struct Query query __attribute__((__cleanup__(query_free))) = {0};
	
	add_parameter(&query, "grant_type", "password");
	add_parameter(&query, "username", user);
	add_parameter(&query, "password", pass);
	
	char* post_fields __attribute__((__cleanup__(charpp_free))) = NULL;
	const int code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_fields);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_URL, HOTMART_TOKEN_ENDPOINT);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "access_token");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const access_token = json_string_value(obj);
	
	obj = json_object_get(tree, "refresh_token");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const refresh_token = json_string_value(obj);
	
	obj = json_object_get(tree, "expires_in");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_integer(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const int expires_in = json_integer_value(obj);
	
	credentials->expires_in = expires_in;
	
	credentials->access_token = malloc(strlen(access_token) + 1);
	credentials->refresh_token = malloc(strlen(refresh_token) + 1);
	
	if (credentials->access_token == NULL || credentials->refresh_token == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->access_token, access_token);
	strcpy(credentials->refresh_token, refresh_token);
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, NULL);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, NULL);
	
	return UERR_SUCCESS;
	
}

static int get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	struct Query query __attribute__((__cleanup__(query_free))) = {0};
	
	add_parameter(&query, "token", credentials->access_token);
	
	char* squery __attribute__((__cleanup__(charpp_free))) = NULL;
	const int code = query_stringify(query, &squery);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU* cu __attribute__((__cleanup__(curlupp_free))) = curl_url();
	curl_url_set(cu, CURLUPART_URL, HOTMART_TOKEN_CHECK_ENDPOINT, 0);
	curl_url_set(cu, CURLUPART_QUERY, squery, 0);
	
	char* url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
	curl_url_get(cu, CURLUPART_URL, &url, 0);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "resources");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	size_t index = 0;
	json_t *item = NULL;
	const size_t array_size = json_array_size(obj);
	
	curl_easy_setopt(curl, CURLOPT_URL, HOTMART_MEMBERSHIP_ENDPOINT);
	
	resources->size = sizeof(struct Resource) * array_size;
	resources->items = malloc(resources->size);
	
	if (resources->items == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "resource");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		obj = json_object_get(obj, "subdomain");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const subdomain = json_string_value(obj);
		
		const char* const headers[][2] = {
			{HTTP_HEADER_AUTHORIZATION, authorization},
			{HTTP_HEADER_CLUB, subdomain}
		};
		
		struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
		
		for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
			const char** const header = (const char**) headers[index];
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
		
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		
		if (curl_easy_perform(curl) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
		
		json_auto_t* subtree = json_loads(string.s, 0, NULL);
		
		if (subtree == NULL) {
			return UERR_JSON_CANNOT_PARSE;
		}
		
		obj = json_object_get(subtree, "name");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const name = json_string_value(obj);
		
		struct Resource resource = {
			.name = malloc(strlen(name) + 1),
			.subdomain = malloc(strlen(subdomain) + 1)
		};
		
		if (resource.name == NULL || resource.subdomain == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(resource.name, name);
		strcpy(resource.subdomain, subdomain);
		
		resources->items[resources->offset++] = resource;
	}
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

static int get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	const char* const headers[][2] = {
		{HTTP_HEADER_AUTHORIZATION, authorization},
		{HTTP_HEADER_CLUB, resource->subdomain}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char** const header = (const char**) headers[index];
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
	curl_easy_setopt(curl, CURLOPT_URL, HOTMART_NAVIGATION_ENDPOINT);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "modules");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	size_t index = 0;
	json_t *item = NULL;
	const size_t array_size = json_array_size(obj);
	
	curl_easy_setopt(curl, CURLOPT_URL, HOTMART_MEMBERSHIP_ENDPOINT);
	
	resource->modules.size = sizeof(struct Module) * array_size;
	resource->modules.items = malloc(resource->modules.size);
	
	if (resource->modules.items == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "id");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const id = json_string_value(obj);
		
		obj = json_object_get(item, "name");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const name = json_string_value(obj);
		
		obj = json_object_get(item, "locked");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_boolean(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const int is_locked = json_boolean_value(obj);
		
		struct Module module = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1),
			.is_locked = is_locked
		};
		
		if (module.id == NULL || module.name == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(module.id, id);
		strcpy(module.name, name);
		
		obj = json_object_get(item, "pages");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t page_index = 0;
		json_t *page_item = NULL;
		const size_t array_size = json_array_size(obj);
		
		module.pages.size = sizeof(struct Page) * array_size;
		module.pages.items = malloc(module.pages.size);
		
		if (module.pages.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		json_array_foreach(obj, page_index, page_item) {
			if (!json_is_object(page_item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(page_item, "hash");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const hash = json_string_value(obj);
			
			obj = json_object_get(page_item, "name");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			struct Page page = {
				.id = malloc(strlen(hash) + 1),
				.name = malloc(strlen(name) + 1)
			};
			
			if (page.id == NULL || page.name == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(page.id, hash);
			strcpy(page.name, name);
			
			module.pages.items[module.pages.offset++] = page;
		}
		
		resource->modules.items[resource->modules.offset++] = module;
	}
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

static int get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	const char* const headers[][2] = {
		{HTTP_HEADER_AUTHORIZATION, authorization},
		{HTTP_HEADER_CLUB, resource->subdomain},
		{HTTP_HEADER_REFERER, HOTMART_HOMEPAGE}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char** const header = (const char**) headers[index];
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
	
	char url[strlen(HOTMART_PAGE_ENDPOINT) + strlen(SLASH) + strlen(page->id) + 1];
	strcpy(url, HOTMART_PAGE_ENDPOINT);
	strcat(url, SLASH);
	strcat(url, page->id);
	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "mediasSrc");
	
	if (obj != NULL) {
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t index = 0;
		json_t *item = NULL;
		const size_t array_size = json_array_size(obj);
		
		page->medias.size = sizeof(struct Media) * array_size;
		page->medias.items = malloc(page->medias.size);
		
		if (page->medias.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		json_array_foreach(obj, index, item) {
			if (!json_is_object(item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(item, "mediaSrcUrl");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const media_page = json_string_value(obj);
			
			obj = json_object_get(item, "mediaName");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const media_name = json_string_value(obj);
			
			struct String string __attribute__((__cleanup__(string_free))) = {0};
			
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
			curl_easy_setopt(curl, CURLOPT_URL, media_page);
			
			if (curl_easy_perform(curl) != CURLE_OK) {
				return UERR_CURL_FAILURE;
			}
			
			const char* const ptr = strstr(string.s, "mediaAssets");
			
			if (ptr == NULL) {
				return UERR_STRSTR_FAILURE;
			}
			
			const char* const start = strstr(ptr, HTTPS_SCHEME);
			const char* const end = strstr(start, QUOTATION_MARK);
			
			size_t size = (size_t) (end - start);
			
			char url[size + 1];
			memcpy(url, start, size);
			url[size] = '\0';
			
			for (size_t index = 0; index < size; index++) {
				char* offset = &url[index];
				
				if (size > (index + 6) && memcmp(offset, "\\u", 2) == 0) {
					const char c1 = from_hex(*(offset + 4));
					const char c2 = from_hex(*(offset + 5));
					
					*offset = (char) ((c1 << 4) | c2);
					memmove(offset + 1, offset + 6, strlen(offset + 6) + 1);
					
					size -= 5;
				}
			}
			
			const char* const file_extension = get_file_extension(media_name);
			
			struct Media media = {
				.type = MEDIA_M3U8,
				.filename = malloc(strlen(media_name) + (file_extension == NULL ? strlen(DOT) + strlen(MP4_FILE_EXTENSION) : 0) + 1),
				.url = malloc(strlen(url) + 1)
			};
			
			if (media.filename == NULL || media.url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(media.url, url);
			strcpy(media.filename, media_name);
			
			if (file_extension == NULL) {
				strcat(media.filename, DOT);
				strcat(media.filename, MP4_FILE_EXTENSION);
			}
			
			normalize_filename(media.filename);
			
			page->medias.items[page->medias.offset++] = media;
		}
		
	}
	
	obj = json_object_get(tree, "attachments");
	
	if (obj != NULL) {
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t index = 0;
		json_t *item = NULL;
		const size_t array_size = json_array_size(obj);
		
		page->attachments.size = sizeof(struct Attachment) * array_size;
		page->attachments.items = malloc(page->attachments.size);
		
		if (page->attachments.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		json_array_foreach(obj, index, item) {
			if (!json_is_object(item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(item, "fileName");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const filename = json_string_value(obj);
			
			obj = json_object_get(item, "fileMembershipId");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const id = json_string_value(obj);
			
			char url[strlen(HOTMART_ATTACHMENT_ENDPOINT) + strlen(SLASH) + strlen(id) + strlen(SLASH) + 8 + 1];
			strcpy(url, HOTMART_ATTACHMENT_ENDPOINT);
			strcat(url, SLASH);
			strcat(url, id);
			strcat(url, SLASH);
			strcat(url, "download");
			
			struct String string __attribute__((__cleanup__(string_free))) = {0};
			
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
			curl_easy_setopt(curl, CURLOPT_URL, url);
			
			if (curl_easy_perform(curl) != CURLE_OK) {
				return UERR_CURL_FAILURE;
			}
			
			json_auto_t* subtree = json_loads(string.s, 0, NULL);
			
			string_free(&string);
			
			if (tree == NULL) {
				return UERR_JSON_CANNOT_PARSE;
			}
			
			const char* download_url = NULL;
			
			obj = json_object_get(subtree, "token");
			
			if (obj == NULL) {
				obj = json_object_get(subtree, "directDownloadUrl");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				download_url = json_string_value(obj);
			} else {
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const drm_token = json_string_value(obj);
				
				obj = json_object_get(subtree, "lambdaUrl");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const lambda_url = json_string_value(obj);
				
				struct curl_slist* sublist __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
				
				char header[strlen(HTTP_HEADER_TOKEN) + strlen(HTTP_HEADER_SEPARATOR) + strlen(drm_token) + 1];
				strcpy(header, HTTP_HEADER_TOKEN);
				strcat(header, HTTP_HEADER_SEPARATOR);
				strcat(header, drm_token);
				
				struct curl_slist* tmp = curl_slist_append(sublist, header);
				
				if (tmp == NULL) {
					return UERR_CURL_FAILURE;
				}
				
				sublist = tmp;
				
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sublist);
				curl_easy_setopt(curl, CURLOPT_URL, lambda_url);
				
				if (curl_easy_perform(curl) != CURLE_OK) {
					return UERR_CURL_FAILURE;
				}
				
				if (!(string.slength > strlen(HTTPS_SCHEME) && memcmp(string.s, HTTPS_SCHEME, strlen(HTTPS_SCHEME)) == 0)) {
					return UERR_ATTACHMENT_DRM_FAILURE;
				}
				
				download_url = string.s;
				
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
				curl_easy_setopt(curl, CURLOPT_URL, NULL);
			}
			
			struct Attachment attachment = {
				.filename = malloc(strlen(filename) + 1),
				.url = malloc(strlen(download_url) + 1),
			};
			
			if (attachment.filename == NULL || attachment.url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(attachment.url, download_url);
			strcpy(attachment.filename, filename);
			
			normalize_filename(attachment.filename);
			
			page->attachments.items[page->attachments.offset++] = attachment;
		}
	}
	
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
	
	obj = json_object_get(tree, "content");
	
	if (obj != NULL) {
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const content = json_string_value(obj);
		
		struct EmbedStream embed_stream __attribute__((__cleanup__(embed_stream_free))) = {0};
		const int code = embed_stream_find(content, &embed_stream);
		
		if (code == UERR_SUCCESS) {
			switch (embed_stream.type) {
				case EMBED_STREAM_VIMEO: {
					curl_easy_setopt(curl, CURLOPT_URL, embed_stream.uri);
					
					struct String string __attribute__((__cleanup__(string_free))) = {0};
					
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
					
					curl_easy_setopt(curl, CURLOPT_REFERER, HOTMART_HOMEPAGE);
					
					if (curl_easy_perform(curl) != CURLE_OK) {
						return UERR_CURL_FAILURE;
					}
					
					const size_t size = page->medias.size + sizeof(struct Media) * 1;
					struct Media* items = (struct Media*) realloc(page->medias.items, size);
					
					if (items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					page->medias.size = size;
					page->medias.items = items;
					
					if (page->medias.items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					struct Media media = {0};
					const int code = vimeo_parse(string.s, &media);
					
					if (code != UERR_SUCCESS) {
						return code;
					}
					
					page->medias.items[page->medias.offset++] = media;
				}
				
				break;
				case EMBED_STREAM_YOUTUBE: {
					const size_t size = page->medias.size + sizeof(struct Media) * 1;
					struct Media* items = (struct Media*) realloc(page->medias.items, size);
					
					if (items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					page->medias.size = size;
					page->medias.items = items;
					
					if (page->medias.items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					struct Media media = {0};
					const int code = youtube_parse(curl, embed_stream.uri, &media);
					
					if (code != UERR_SUCCESS) {
						return code;
					}
					
					page->medias.items[page->medias.offset++] = media;
				};
				break;
			}
		} else if (code != UERR_NO_STREAMS_AVAILABLE) {
			return code;
		}
		
		page->document.filename = malloc(strlen(page->name) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
		page->document.content = malloc(strlen(HTML_HEADER_START) + strlen(content) + strlen(HTML_HEADER_END) + 1);
		
		if (page->document.filename == NULL || page->document.content == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(page->document.filename, page->name);
		strcat(page->document.filename, DOT);
		strcat(page->document.filename, HTML_FILE_EXTENSION);
		
		normalize_filename(page->document.filename);
		
		strcpy(page->document.content, HTML_HEADER_START);
		strcat(page->document.content, content);
		strcat(page->document.content, HTML_HEADER_END);
	}
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl, CURLOPT_URL, NULL);
	curl_easy_setopt(curl, CURLOPT_REFERER, NULL);
	
	return UERR_SUCCESS;
	
}

static int ask_user_credentials(struct Credentials* const obj) {
	
	char username[MAX_INPUT_SIZE + 1] = {'\0'};
	char password[MAX_INPUT_SIZE + 1] = {'\0'};
	
	while (1) {
		printf("> Insira seu usuário: ");
		
		if (fgets(username, sizeof(username), stdin) != NULL && *username != '\n') {
			break;
		}
		
		fprintf(stderr, "- Usuário inválido ou não reconhecido!\r\n");
	}
	
	*strchr(username, '\n') = '\0';
	
	while (1) {
		printf("> Insira sua senha: ");
		
		if (fgets(password, sizeof(password), stdin) != NULL && *password != '\n') {
			break;
		}
		
		fprintf(stderr, "- Senha inválida ou não reconhecida!\r\n");
	}
	
	*strchr(password, '\n') = '\0';
	
	if (authorize(username, password, obj) != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível realizar a autenticação!\r\n");
		return 0;
	}
	
	obj->username = malloc(strlen(username) + 1);
	
	if (obj->username == NULL) {
		
	}
	
	strcpy(obj->username, username);
	
	printf("+ Usuário autenticado com sucesso!\r\n");
	
	return 1;
	
}

#if defined(WIN32) && defined(UNICODE)
	#define main wmain
#endif

int main() {
	
	#ifdef WIN32
		_setmaxstdio(2048);
		
		#ifdef UNICODE
			_setmode(_fileno(stdout), _O_WTEXT);
			_setmode(_fileno(stderr), _O_WTEXT);
			
			setlocale(LC_ALL, ".UTF8");
		#endif
	#else
		struct rlimit rlim = {0};
		getrlimit(RLIMIT_NOFILE, &rlim);
		
		rlim.rlim_cur = rlim.rlim_max;
		setrlimit(RLIMIT_NOFILE, &rlim);
	#endif
	
	if (is_administrator()) {
		fprintf(stderr, "- Você não precisa e nem deve executar este programa com privilégios elevados!\r\n");
		return EXIT_FAILURE;
	}
	
	char* const directory = get_configuration_directory();
	
	if (directory == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return EXIT_FAILURE;
	}
	
	char configuration_directory[strlen(directory) + strlen(A) + 1];
	strcpy(configuration_directory, directory);
	strcat(configuration_directory, A);
	
	free(directory);
	
	if (!directory_exists(configuration_directory)) {
		fprintf(stderr, "- Diretório de configurações não encontrado, criando-o\r\n");
		
		if (!create_directory(configuration_directory)) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
	}
	
	char accounts_file[strlen(configuration_directory) + strlen(PATH_SEPARATOR) + strlen(LOCAL_ACCOUNTS_FILENAME) + 1];
	strcpy(accounts_file, configuration_directory);
	strcat(accounts_file, PATH_SEPARATOR);
	strcat(accounts_file, LOCAL_ACCOUNTS_FILENAME);
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	CURLM* multi_handle = curl_multi_init();
	
	if (multi_handle == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return EXIT_FAILURE;
	}
	
	curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, (long) 30);
	curl_multi_setopt(multi_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long) 30);
	
	curl = curl_easy_init();
	
	if (curl == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return EXIT_FAILURE;
	}
	
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L);
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	struct curl_blob blob = {
		.data = (char*) CACERT,
		.len = strlen(CACERT),
		.flags = CURL_BLOB_COPY
	};
	
	curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
	
	struct Credentials credentials = {0};
	
	if (file_exists(accounts_file)) {
		json_auto_t* tree = json_load_file(accounts_file, 0, NULL);
		
		if (tree == NULL || !json_is_array(tree)) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		const size_t total_items = json_array_size(tree);
		
		if (total_items < 1) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		struct Credentials items[total_items];
		
		size_t index = 0;
		json_t *item = NULL;
		
		printf("+ Selecione qual das suas contas você deseja usar: \r\n\r\n");
		printf("0.\r\nAcessar uma outra conta\r\n\r\n");
		
		json_array_foreach(tree, index, item) {
			json_t* subobj = json_object_get(item, "username");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
			const char* const username = json_string_value(subobj);
			
			subobj = json_object_get(item, "access_token");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
			const char* const access_token = json_string_value(subobj);
			
			subobj = json_object_get(item, "refresh_token");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
			const char* const refresh_token = json_string_value(subobj);
			
			struct Credentials credentials = {
				.access_token = malloc(strlen(access_token) + 1),
				.refresh_token = malloc(strlen(refresh_token) + 1)
			};
			
			if (credentials.access_token == NULL || credentials.refresh_token == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
			strcpy(credentials.access_token, access_token);
			strcpy(credentials.refresh_token, refresh_token);
			
			items[index] = credentials;
			
			printf("%zu. \r\nAcessar usando a conta: '%s'\r\n\r\n", index + 1, username);
		}
		
		char answer[4 + 1];
		int value = 0;
		
		while (1) {
			printf("> Digite sua escolha: ");
			
			if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
				*strchr(answer, '\n') = '\0';
				
				if (isnumeric(answer)) {
					value = atoi(answer);
					
					if (value >= 0 && (size_t) value <= total_items) {
						break;
					}
				}
			}
			
			fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
		}
		
		if (value == 0) {
			if (!ask_user_credentials(&credentials)) {
				return EXIT_FAILURE;
			}
			
			FILE* const file = fopen(accounts_file, "wb");
			
			if (file == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
			json_auto_t* subtree = json_object();
			json_object_set_new(subtree, "username", json_string(credentials.username));
			json_object_set_new(subtree, "access_token", json_string(credentials.access_token));
			json_object_set_new(subtree, "refresh_token", json_string(credentials.refresh_token));
			
			json_array_append(tree, subtree);
			
			char* const buffer = json_dumps(tree, JSON_COMPACT);
			
			if (buffer == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
			const size_t buffer_size = strlen(buffer);
			const size_t wsize = fwrite(buffer, sizeof(*buffer), buffer_size, file);
			
			free(buffer);
			fclose(file);
			
			if (wsize != buffer_size) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return EXIT_FAILURE;
			}
			
		} else {
			credentials = items[value - 1];
		}
	} else {
		if (!ask_user_credentials(&credentials)) {
			return EXIT_FAILURE;
		}
		
		FILE* const file = fopen(accounts_file, "wb");
		
		if (file == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		json_auto_t* tree = json_array();
		json_auto_t* obj = json_object();
		
		if (tree == NULL || obj == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		json_object_set_new(obj, "username", json_string(credentials.username));
		json_object_set_new(obj, "access_token", json_string(credentials.access_token));
		json_object_set_new(obj, "refresh_token", json_string(credentials.refresh_token));
		
		json_array_append(tree, obj);
		
		char* const buffer = json_dumps(tree, JSON_COMPACT);
		
		if (buffer == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		const size_t buffer_size = strlen(buffer);
		const size_t wsize = fwrite(buffer, sizeof(*buffer), buffer_size, file);
		
		free(buffer);
		fclose(file);
		
		if (wsize != buffer_size) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
	}
	
	printf("+ Obtendo lista de produtos\r\n");
	
	struct Resources resources = {0};
	
	if (get_resources(&credentials, &resources) != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível obter a lista de produtos!\r\n");
		return EXIT_FAILURE;
	}
	
	printf("+ Selecione o que deseja baixar:\r\n\r\n");
	
	printf("0.\r\nTodos os produtos disponíveis\r\n\r\n");
	
	for (size_t index = 0; index < resources.offset; index++) {
		const struct Resource* resource = &resources.items[index];
		printf("%zu. \r\nNome: %s\r\nhttps://%s%s\r\n\r\n", index + 1, resource->name, resource->subdomain, HOTMART_CLUB_SUFFIX);
	}
	
	char answer[4 + 1];
	int value = 0;
	
	while (1) {
		printf("> Digite sua escolha: ");
		
		if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
			*strchr(answer, '\n') = '\0';
			
			if (isnumeric(answer)) {
				value = atoi(answer);
				
				if (value >= 0 && (size_t) value <= resources.offset) {
					break;
				}
			}
		}
		
		fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
	}
	
	fclose(stdin);
	
	struct Resource download_queue[resources.offset];
	
	size_t queue_count = 0;
	
	if (value == 0) {
		for (size_t index = 0; index < sizeof(download_queue) / sizeof(*download_queue); index++) {
			struct Resource resource = resources.items[index];
			download_queue[index] = resource;
			
			queue_count++;
		}
	} else {
		struct Resource resource = resources.items[value - 1];
		*download_queue = resource;
		
		queue_count++;
	}
	
	char* cwd = get_current_directory();
	
	if (cwd == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return EXIT_FAILURE;
	}
	
	for (size_t index = 0; index < queue_count; index++) {
		struct Resource* resource = &download_queue[index];
		
		printf("+ Obtendo lista de módulos do produto '%s'\r\n", resource->name);
		
		if (get_modules(&credentials, resource) != UERR_SUCCESS) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		char directory[strlen(resource->name) + 1];
		strcpy(directory, resource->name);
		normalize_filename(directory);
		
		char resource_directory[strlen(cwd) + strlen(PATH_SEPARATOR) + strlen(directory) + 1];
		strcpy(resource_directory, cwd);
		strcat(resource_directory, PATH_SEPARATOR);
		strcat(resource_directory, directory);
		
		if (!directory_exists(resource_directory)) {
			fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", resource_directory);
			
			if (!create_directory(resource_directory)) {
				fprintf(stderr, "- Ocorreu um erro ao tentar criar o diretório!\r\n");
				return EXIT_FAILURE;
			}
		}
		
		for (size_t index = 0; index < resource->modules.offset; index++) {
			struct Module* module = &resource->modules.items[index];
			
			printf("+ Verificando estado do módulo '%s'\r\n", module->name);
			
			if (module->is_locked) {
				fprintf(stderr, "- Módulo inacessível, pulando para o próximo\r\n");
				continue;
			}
			
			char directory[strlen(module->name) + 1];
			strcpy(directory, module->name);
			normalize_filename(directory);
			
			char module_directory[strlen(resource_directory) + strlen(PATH_SEPARATOR) + strlen(directory) + 1];
			strcpy(module_directory, resource_directory);
			strcat(module_directory, PATH_SEPARATOR);
			strcat(module_directory, directory);
			
			if (!directory_exists(module_directory)) {
				fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", module_directory);
				
				if (!create_directory(module_directory)) {
					fprintf(stderr, "- Ocorreu um erro ao tentar criar o diretório!\r\n");
					return EXIT_FAILURE;
				}
			}
			
			printf("+ Obtendo lista de páginas do módulo '%s'\r\n", module->name);
			
			for (size_t index = 0; index < module->pages.offset; index++) {
				struct Page* page = &module->pages.items[index];
				
				if (get_page(&credentials, resource, page) != UERR_SUCCESS) {
					fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
					return EXIT_FAILURE;
				}
				
				printf("+ Verificando estado da página '%s'\r\n", page->name);
				
				char directory[strlen(page->name) + 1];
				strcpy(directory, page->name);
				normalize_filename(directory);
				
				char page_directory[strlen(module_directory) + strlen(PATH_SEPARATOR) + strlen(directory) + 1];
				strcpy(page_directory, module_directory);
				strcat(page_directory, PATH_SEPARATOR);
				strcat(page_directory, directory);
				
				if (!directory_exists(page_directory)) {
					fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", page_directory);
					
					if (!create_directory(page_directory)) {
						fprintf(stderr, "- Ocorreu um erro ao tentar criar o diretório!\r\n");
						return EXIT_FAILURE;
					}
				}
				
				if (page->document.content != NULL) {
					char document_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(page->document.filename) + 1];
					strcpy(document_filename, page_directory);
					strcat(document_filename, PATH_SEPARATOR);
					strcat(document_filename, page->document.filename);
					
					if (!file_exists(document_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, salvando-o\r\n", document_filename);
						
						FILE* const stream = fopen(document_filename, "wb");
						
						if (stream == NULL) {
							fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
							return EXIT_FAILURE;
						}
						
						const size_t size = strlen(page->document.content);
						const size_t written = fwrite(page->document.content, sizeof(*page->document.content), size, stream);
						
						fclose(stream);
						
						if (written != size) {
							remove_file(document_filename);
							fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
							return EXIT_FAILURE;
						}
					}
				}
				
				for (size_t index = 0; index < page->medias.offset; index++) {
					struct Media* media = &page->medias.items[index];
					
					char media_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(media->filename) + 1];
					strcpy(media_filename, page_directory);
					strcat(media_filename, PATH_SEPARATOR);
					strcat(media_filename, media->filename);
					
					if (!file_exists(media_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, ele será baixado\r\n", media_filename);
						printf("+ Baixando de '%s' para '%s'\r\n", media->url, media_filename);
						
						switch (media->type) {
							case MEDIA_M3U8: {
								struct String string __attribute__((__cleanup__(string_free))) = {0};
								
								curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
								curl_easy_setopt(curl, CURLOPT_URL, media->url);
								curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
								curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
								
								if (curl_easy_perform(curl) != CURLE_OK) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								struct Tags tags = {0};
								
								if (m3u8_parse(&tags, string.s) != UERR_SUCCESS) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								int last_width = 0;
								const char* playlist_uri = NULL;
								
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
										playlist_uri = tag->uri;
									}
								}
								
								CURLU* cu __attribute__((__cleanup__(curlupp_free))) = curl_url();
								curl_url_set(cu, CURLUPART_URL, media->url, 0);
								curl_url_set(cu, CURLUPART_URL, playlist_uri, 0);
								
								char* playlist_full_url __attribute__((__cleanup__(curlcharpp_free))) = NULL;	
								curl_url_get(cu, CURLUPART_URL, &playlist_full_url, 0);
								
								m3u8_free(&tags);
								string_free(&string);
								
								curl_easy_setopt(curl, CURLOPT_URL, playlist_full_url);
								
								if (curl_easy_perform(curl) != CURLE_OK) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								if (m3u8_parse(&tags, string.s) != UERR_SUCCESS) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								int segment_number = 1;
								
								struct SegmentDownload downloads[tags.offset];
								size_t downloads_offset = 0;
								
								char playlist_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(LOCAL_PLAYLIST_FILENAME) + 1];
								strcpy(playlist_filename, page_directory);
								strcat(playlist_filename, PATH_SEPARATOR);
								strcat(playlist_filename, LOCAL_PLAYLIST_FILENAME);
								
								for (size_t index = 0; index < tags.offset; index++) {
									struct Tag* tag = &tags.items[index];
									
									if (tag->type == EXT_X_KEY) {
										struct Attribute* attribute = attributes_get(&tag->attributes, "URI");
										
										curl_url_set(cu, CURLUPART_URL, attribute->value, 0);
										
										char* url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
										curl_url_get(cu, CURLUPART_URL, &url, 0);
										
										char* filename = malloc(strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(KEY_FILE_EXTENSION) + strlen(DOT) + strlen(KEY_FILE_EXTENSION) + 1);
										strcpy(filename, page_directory);
										
										strcat(filename, PATH_SEPARATOR);
										strcat(filename, KEY_FILE_EXTENSION);
										strcat(filename, DOT);
										strcat(filename, KEY_FILE_EXTENSION);
										
										attribute_set_value(attribute, filename);
										
										CURL* handle = curl_easy_init();
										
										if (handle == NULL) {
											fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
											return EXIT_FAILURE;
										}
										
										curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
										curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
										curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
										curl_easy_setopt(handle, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
										curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
										curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &blob);
										curl_easy_setopt(handle, CURLOPT_URL, url);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 30L);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 15L);
										
										FILE* stream = fopen(filename, "wb");
										
										if (stream == NULL) {
											fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
											return EXIT_FAILURE;
										}
										
										curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*) stream);
										curl_multi_add_handle(multi_handle, handle);
										
										struct SegmentDownload download = {
											.handle = handle,
											.filename = filename,
											.stream = stream
										};
										
										downloads[downloads_offset++] = download;
										
										curl_url_set(cu, CURLUPART_URL, playlist_full_url, 0);
										curl_url_set(cu, CURLUPART_URL, tag->uri, 0);
										
										char* segment_url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
										curl_url_get(cu, CURLUPART_URL, &segment_url, 0);
										
										handle = curl_easy_init();
										
										if (handle == NULL) {
											fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
											return EXIT_FAILURE;
										}
										
										curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
										curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
										curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
										curl_easy_setopt(handle, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
										curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
										curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &blob);
										curl_easy_setopt(handle, CURLOPT_URL, segment_url);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 30L);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 15L);
										
										char value[intlen(segment_number) + 1];
										snprintf(value, sizeof(value), "%i", segment_number);
										
										char* segment_filename = malloc(strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(value) + strlen(DOT) + strlen(TS_FILE_EXTENSION) + 1);
										strcpy(segment_filename, page_directory);
										strcat(segment_filename, PATH_SEPARATOR);
										strcat(segment_filename, value);
										strcat(segment_filename, DOT);
										strcat(segment_filename, TS_FILE_EXTENSION);
										
										tag_set_uri(tag, segment_filename);
										
										stream = fopen(segment_filename, "wb");
										
										if (stream == NULL) {
											fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
											return EXIT_FAILURE;
										}
										
										curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*) stream);
										curl_multi_add_handle(multi_handle, handle);
										
										download = (struct SegmentDownload) {
											.handle = handle,
											.filename = segment_filename,
											.stream = stream
										};
										
										downloads[downloads_offset++] = download;
										
										segment_number++;
									} else if (tag->type == EXTINF && tag->uri != NULL) {
										curl_url_set(cu, CURLUPART_URL, tag->uri, 0);
										
										char* url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
										curl_url_get(cu, CURLUPART_URL, &url, 0);
										
										char value[intlen(segment_number) + 1];
										snprintf(value, sizeof(value), "%i", segment_number);
										
										char* filename = malloc(strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(value) + strlen(DOT) + strlen(TS_FILE_EXTENSION) + 1);
										strcpy(filename, page_directory);
										strcat(filename, PATH_SEPARATOR);
										strcat(filename, value);
										strcat(filename, DOT);
										strcat(filename, TS_FILE_EXTENSION);
										
										tag_set_uri(tag, filename);
										
										CURL* handle = curl_easy_init();
										
										if (handle == NULL) {
											fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
											return EXIT_FAILURE;
										}
										
										curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
										curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
										curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
										curl_easy_setopt(handle, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
										curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
										curl_easy_setopt(handle, CURLOPT_CAPATH, NULL);
										curl_easy_setopt(handle, CURLOPT_CAINFO, NULL);
										curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &blob);
										curl_easy_setopt(handle, CURLOPT_URL, url);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 30L);
										curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 15L);
										
										FILE* const stream = fopen(filename, "wb");
										
										if (stream == NULL) {
											fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
											return EXIT_FAILURE;
										}
										
										curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*) stream);
										curl_multi_add_handle(multi_handle, handle);
										
										struct SegmentDownload download = {
											.handle = handle,
											.filename = filename,
											.stream = stream
										};
										
										downloads[downloads_offset++] = download;
										
										segment_number++;
									}
									
								}
								
								int still_running = 1;
								int done = 0;
								
								progress_callback(NULL, downloads_offset, done, 0, 0);
								
								while (still_running) {
									CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
									
									if (still_running) {
										mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);
									}
										
									CURLMsg* msg = NULL;
									int msgs_left = 0;
									
									while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
										if (msg->msg == CURLMSG_DONE) {
											struct SegmentDownload* download = NULL;
											
											for (size_t index = 0; index < downloads_offset; index++) {
												struct SegmentDownload* subdownload = &downloads[index];
												
												if (subdownload->handle == msg->easy_handle) {
													download = subdownload;
													break;
												}
											}
											
											curl_multi_remove_handle(multi_handle, msg->easy_handle);
											
											if (msg->data.result == CURLE_OK) {
												curl_easy_cleanup(msg->easy_handle);
												fclose(download->stream);
												download->stream = NULL;
												
												done++;
												progress_callback(NULL, downloads_offset, done, 0, 0);
											} else {
												fseek(download->stream, 0, SEEK_SET);
												curl_multi_add_handle(multi_handle, msg->easy_handle);
											}
										}
									}
									
									if (mc) {
										break;
									}
								}
								
								printf("\r\n");
								
								printf("+ Exportando lista de reprodução para '%s'\r\n", playlist_filename);
								 
								FILE* const stream = fopen(playlist_filename, "wb");
								
								if (stream == NULL) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								const int ok = tags_dumpf(&tags, stream);
								
								fclose(stream);
								m3u8_free(&tags);
								
								if (!ok) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								char output_file[strlen(QUOTATION_MARK) * 2 + strlen(media_filename) + 1];
								strcpy(output_file, QUOTATION_MARK);
								strcat(output_file, media_filename);
								strcat(output_file, QUOTATION_MARK);
								
								printf("+ Copiando arquivos de mídia para '%s'\r\n", media_filename);
								
								const char* const command[][2] = {
									{"ffmpeg", NULL},
									{"-loglevel", "error"},
									{"-allowed_extensions", "ALL"},
									{"-i", playlist_filename},
									{"-c", "copy"},
									{"-movflags", "+faststart"},
									{"-map_metadata", "-1"},
									{output_file, NULL}
								};
								
								char command_line[5000];
								memset(command_line, '\0', sizeof(command_line));
								
								for (size_t index = 0; index < sizeof(command) / sizeof(*command); index++) {
									const char** const argument = (const char**) command[index];
									
									const char* const key = argument[0];
									const char* const value = argument[1];
									
									if (key != NULL) {
										strcat(command_line, key);
									}
									
									strcat(command_line, SPACE);
									
									if (value != NULL) {
										strcat(command_line, QUOTATION_MARK);
										strcat(command_line, value);
										strcat(command_line, QUOTATION_MARK);
									}
									
									strcat(command_line, SPACE);
								}
								
								const int exit_code = execute_shell_command(command_line);
								
								for (size_t index = 0; index < downloads_offset; index++) {
									struct SegmentDownload* download = &downloads[index];
									
									remove_file(download->filename);
									free(download->filename);
								}
								
								remove_file(playlist_filename);
								
								if (exit_code != 0) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
							}
							
							curl_easy_setopt(curl, CURLOPT_URL, NULL);
							curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
							curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
							
							break;
							
							case MEDIA_SINGLE: {
								FILE* const stream = fopen(media_filename, "wb");
								
								if (stream == NULL) {
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
								
								curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
								curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
								curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
								curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) stream);
								curl_easy_setopt(curl, CURLOPT_URL, media->url);
								curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
								
								const CURLcode code = curl_easy_perform(curl);
								
								fclose(stream);
								
								curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, NULL);
								curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, NULL);
								curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
								curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
								curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
								curl_easy_setopt(curl, CURLOPT_URL, NULL);
								curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
								
								printf("\r\n");
								
								if (code != CURLE_OK) {
									remove_file(media_filename);
									fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
									return EXIT_FAILURE;
								}
							}
							
							break;
						}
					}
				}
				
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
				curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
				
				for (size_t index = 0; index < page->attachments.offset; index++) {
					struct Attachment* attachment = &page->attachments.items[index];
					
					char attachment_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + strlen(attachment->filename) + 1];
					strcpy(attachment_filename, page_directory);
					strcat(attachment_filename, PATH_SEPARATOR);
					strcat(attachment_filename, attachment->filename);
					
					if (!file_exists(attachment_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, ele será baixado\r\n", attachment_filename);
						printf("+ Baixando de '%s' para '%s'\r\n", attachment->url, attachment_filename);
						
						FILE* const stream = fopen(attachment_filename, "wb");
						
						if (stream == NULL) {
							fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
							return EXIT_FAILURE;
						}
						
						curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
						curl_easy_setopt(curl, CURLOPT_URL, attachment->url);
						curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) stream);
						
						const CURLcode code = curl_easy_perform(curl);
						
						fclose(stream);
						
						printf("\r\n");
						
						if (code != CURLE_OK) {
							remove_file(attachment_filename);
							fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
							return EXIT_FAILURE;
						}
					}
				}
				
				curl_easy_setopt(curl, CURLOPT_URL, NULL);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
				curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
				curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, NULL);
				
			}
			
		}
		
	}
	
	return 0;
}
