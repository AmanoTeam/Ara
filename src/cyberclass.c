#include <stdio.h>

#include <curl/curl.h>
#include <jansson.h>

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#include "credentials.h"
#include "resources.h"
#include "cleanup.h"
#include "errors.h"
#include "query.h"
#include "callbacks.h"
#include "types.h"
#include "stringu.h"
#include "symbols.h"
#include "vimeo.h"
#include "curl.h"
#include "cyberclass.h"

static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";
static const char HTTP_HEADER_CONTENT_TYPE[] = "Content-Type";

static const char HTTP_AUTHENTICATION_BEARER[] = "Bearer";

static const char CONTENT_TYPE_JSON[] = "application/json";

static const char COURSES[] = "courses";

#define CYBERCLASS_HOMEPAGE_ENDPOINT "https://www.cyberclass.com.br"
#define CYBERCLASS_API_ENDPOINT "https://api.cyberclass.com.br/api/v1"

static const char CYBERCLASS_LOGIN_ENDPOINT[] = 
	CYBERCLASS_API_ENDPOINT
	"/auth/login";

static const char CYBERCLASS_COURSE_CATEGORIES_ENDPOINT[] = 
	CYBERCLASS_API_ENDPOINT
	"/course-categories";

static const char CYBERCLASS_COURSE_ENDPOINT[] = 
	CYBERCLASS_API_ENDPOINT
	"/courses";

static const char CYBERCLASS_COURSE_HOMEPAGE[] = 
	CYBERCLASS_HOMEPAGE_ENDPOINT
	"/curso";

static const char VIMEO_PLAYER_PREFIX[] = "https://player.vimeo.com/video";

int cyberclass_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	json_auto_t* tree = json_object();
	
	json_object_set_new(tree, "email", json_string(username));
	json_object_set_new(tree, "password", json_string(password));
	
	char* post_fields __attribute__((__cleanup__(charpp_free))) = json_dumps(tree, JSON_COMPACT);
	
	if (post_fields == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	const char* const headers[][2] = {
		{HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE_JSON}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char* const* const header = headers[index];
		
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, CYBERCLASS_LOGIN_ENDPOINT);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* subtree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* data = json_object_get(subtree, "data");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* obj = json_object_get(data, "token");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* access_token = json_string_value(obj);
	
	obj = json_object_get(data, "user");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "name");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const name = json_string_value(obj);
	
	credentials->username = malloc(strlen(name) + 1);
	credentials->access_token = malloc(strlen(access_token) + 1);
	
	if (credentials->username == NULL || credentials->access_token == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->username, name);
	strcpy(credentials->access_token, access_token);
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	
	return UERR_SUCCESS;
	
}

int cyberclass_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	const char* const headers[][2] = {
		{HTTP_HEADER_AUTHORIZATION, authorization}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char* const* const header = headers[index];
		
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, CYBERCLASS_COURSE_CATEGORIES_ENDPOINT);
	
	const CURLcode code = curl_easy_perform(curl_easy);
	
	if (code == CURLE_HTTP_RETURNED_ERROR) {
		return UERR_PROVIDER_SESSION_EXPIRED;
	}
	
	if (code != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* data = json_object_get(tree, "data");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	size_t index = 0;
	const json_t* item = NULL;
	
	jint_array_t ids __attribute__((__cleanup__(jint_array_free))) = {0};
	
	json_array_foreach(data, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "categories");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const size_t array_size = json_array_size(obj);
		
		if (array_size > 0) {
			const size_t size = ids.size + (sizeof(json_int_t) * array_size);
			json_int_t* items = realloc(ids.items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			ids.size = size;
			ids.items = items;
			
			size_t index = 0;
			const json_t* item = NULL;
			
			json_array_foreach(obj, index, item) {
				if (!json_is_object(item)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const json_t* obj = json_object_get(item, "id");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_integer(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const json_int_t id = json_integer_value(obj);
				
				ids.items[ids.offset++] = id;
			}
		} else {
			const size_t size = ids.size + (sizeof(json_int_t) * 1);
			json_int_t* items = realloc(ids.items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			ids.size = size;
			ids.items = items;
			
			const json_t* obj = json_object_get(item, "id");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_integer(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_int_t id = json_integer_value(obj);
			
			ids.items[ids.offset++] = id;
		}
	}
	
	for (size_t index = 0; index < ids.offset; index++) {
		const json_int_t id = ids.items[index];
		
		char sid[intlen(id) + 1];
		snprintf(sid, sizeof(sid), "%llu", id);
		
		char url[strlen(CYBERCLASS_COURSE_CATEGORIES_ENDPOINT) + strlen(SLASH) + strlen(sid) + strlen(SLASH) + strlen(COURSES) + 1];
		strcpy(url, CYBERCLASS_COURSE_CATEGORIES_ENDPOINT);
		strcat(url, SLASH);
		strcat(url, sid);
		strcat(url, SLASH);
		strcat(url, COURSES);
		
		curl_easy_setopt(curl_easy, CURLOPT_URL, url);
		
		while (1) {
			struct String string __attribute__((__cleanup__(string_free))) = {0};
			
			curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
			
			const CURLcode code = curl_easy_perform(curl_easy);
			
			if (code == CURLE_HTTP_RETURNED_ERROR) {
				return UERR_PROVIDER_SESSION_EXPIRED;
			}
			
			if (code != CURLE_OK) {
				return UERR_CURL_FAILURE;
			}
			
			json_auto_t* tree = json_loads(string.s, 0, NULL);
			
			if (tree == NULL) {
				return UERR_JSON_CANNOT_PARSE;
			}
			
			const json_t* data = json_object_get(tree, "data");
			
			if (data == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_array(data)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			size_t index = 0;
			const json_t* item = NULL;
			const size_t array_size = json_array_size(data);
			
			const size_t size = resources->size + (sizeof(struct Resource) * array_size);
			struct Resource* items = realloc(resources->items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			resources->size = size;
			resources->items = items;
			
			json_array_foreach(data, index, item) {
				if (!json_is_object(item)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const json_t* obj = json_object_get(item, "id");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_integer(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const json_int_t id = json_integer_value(obj);
				
				char sid[intlen(id) + 1];
				snprintf(sid, sizeof(sid), "%llu", id);
				
				obj = json_object_get(item, "name");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const name = json_string_value(obj);
				
				obj = json_object_get(item, "slug");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const slug = json_string_value(obj);
				
				struct Resource resource = {
					.id = malloc(strlen(sid) + 1),
					.name = malloc(strlen(name) + 1),
					.url = malloc(strlen(CYBERCLASS_COURSE_HOMEPAGE) + strlen(SLASH) + strlen(sid) + strlen(HYPHEN) + strlen(slug) + 1)
				};
				
				if (resource.id == NULL || resource.name == NULL || resource.url == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(resource.id, sid);
				strcpy(resource.name, name);
				
				strcpy(resource.url, CYBERCLASS_COURSE_HOMEPAGE);
				strcat(resource.url, SLASH);
				strcat(resource.url, sid);
				strcat(resource.url, HYPHEN);
				strcat(resource.url, slug);
				
				resources->items[resources->offset++] = resource;
			}
			
			const json_t* obj = json_object_get(tree, "links");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_object(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			obj = json_object_get(obj, "next");
			
			if (json_is_null(obj)) {
				break;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const url = json_string_value(obj);
			
			curl_easy_setopt(curl_easy, CURLOPT_URL, url);
		}
		
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int cyberclass_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	const char* const headers[][2] = {
		{HTTP_HEADER_AUTHORIZATION, authorization}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char* const* const header = headers[index];
		
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
	
	char url[strlen(CYBERCLASS_COURSE_ENDPOINT) + strlen(SLASH) + strlen(resource->id) + 1];
	strcpy(url, CYBERCLASS_COURSE_ENDPOINT);
	strcat(url, SLASH);
	strcat(url, resource->id);
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* data = json_object_get(tree, "data");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* obj = json_object_get(data, "course_modules");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const size_t array_size = json_array_size(obj);
	
	resource->modules.size = sizeof(struct Module) * array_size;
	resource->modules.items = malloc(resource->modules.size);
	
	if (resource->modules.items == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	size_t index = 0;
	const json_t* item = NULL;
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "id");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_integer(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_int_t id = json_integer_value(obj);
		
		char sid[intlen(id) + 1];
		snprintf(sid, sizeof(sid), "%llu", id);
		
		obj = json_object_get(item, "name");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
			
		const char* const name = json_string_value(obj);
		
		obj = json_object_get(item, "is_visible");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_integer(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const int is_locked = !json_integer_value(obj);
		
		struct Module module = {
			.id = malloc(strlen(sid) + 1),
			.name = malloc(strlen(name) + 1),
			.is_locked = is_locked
		};
		
		if (module.id == NULL || module.name == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(module.id, sid);
		strcpy(module.name, name);
		
		obj = json_object_get(item, "course_classes_visible");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t subindex = 0;
		const json_t* subitem = NULL;
		const size_t array_size = json_array_size(obj);
		
		module.pages.size = sizeof(struct Page) * array_size;
		module.pages.items = malloc(module.pages.size);
		
		if (module.pages.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		json_array_foreach(obj, subindex, subitem) {
			if (!json_is_object(subitem)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(subitem, "id");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_integer(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_int_t id = json_integer_value(obj);
			
			char sid[intlen(id) + 1];
			snprintf(sid, sizeof(sid), "%llu", id);
			
			obj = json_object_get(subitem, "name");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			struct Page page = {
				.id = malloc(strlen(sid) + 1),
				.name = malloc(strlen(name) + 1),
				.is_locked = 0
			};
			
			if (page.id == NULL || page.name == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(page.id, sid);
			strcpy(page.name, name);
			
			obj = json_object_get(subitem, "video_provider");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const video_provider = json_string_value(obj);
			
			obj = json_object_get(subitem, "video_code");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const video_code = json_string_value(obj);
			
			page.medias.size = sizeof(struct Media) * 1;
			page.medias.items = malloc(page.medias.size);
			
			if (page.medias.items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
			
			if (strcmp(video_provider, "vimeo") == 0) {
				char url[strlen(VIMEO_PLAYER_PREFIX) + strlen(SLASH) + strlen(video_code) + 1];
				strcpy(url, VIMEO_PLAYER_PREFIX);
				strcat(url, SLASH);
				strcat(url, video_code);
				
				struct Media media = {0};
				const int code = vimeo_parse(url, resource, &page, &media, resource->url);
				
				if (!(code == UERR_SUCCESS || code == UERR_NO_STREAMS_AVAILABLE)) {
					return code;
				}
				
				page.medias.items[page.medias.offset++] = media;
			} else {
				return UERR_UNSUPPORTED_MEDIA_PROVIDER;
			}
			
			curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
			
			module.pages.items[module.pages.offset++] = page;
		}
		
		resource->modules.items[resource->modules.offset++] = module;
	
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int cyberclass_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	(void) credentials;
	(void) resource;
	(void) module;
	
	return UERR_NOT_IMPLEMENTED;
	
}

int cyberclass_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	(void) credentials;
	(void) resource;
	(void) page;
	
	return UERR_NOT_IMPLEMENTED;
	
}