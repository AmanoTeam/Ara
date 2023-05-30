#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <jansson.h>

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#include "credentials.h"
#include "resources.h"
#include "cleanup.h"
#include "errors.h"
#include "callbacks.h"
#include "types.h"
#include "stringu.h"
#include "query.h"
#include "symbols.h"
#include "curl.h"
#include "buffer.h"
#include "kiwify.h"
#include "vimeo.h"

static const char HTTP_HEADER_CONTENT_TYPE[] = "Content-Type";
static const char CONTENT_TYPE_JSON[] = "application/json";
static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";
static const char HTTP_AUTHENTICATION_BEARER[] = "Bearer";
static const char VIMEO_URL_PATTERN[] = "https://player.vimeo.com";

#define GOOGLE_APIS_ENDPOINT "https://www.googleapis.com"
#define KIWIFY_API_ENDPOINT "https://api.kiwify.com.br"
#define KIWIFY_DASHBOARD_HOMEPAGE "https://dashboard.kiwify.com.br"

static const char KIWIFY_LOGIN_ENDPOINT[] = 
	GOOGLE_APIS_ENDPOINT
	"/identitytoolkit/v3/relyingparty/verifyPassword?key=AIzaSyDmOO1YAGt0X35zykOMTlolvsoBkefLKFU";

static const char KIWIFY_COURSES_ENDPOINT[] = 
	KIWIFY_API_ENDPOINT
	"/v1/viewer/courses";

static const char KIWIFY_COURSE_HOMEPAGE[] = 
	KIWIFY_DASHBOARD_HOMEPAGE
	"/course";

static const char VIDEO[] = "video";

int kiwify_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	json_auto_t* tree = json_object();
	
	json_object_set_new(tree, "email", json_string(username));
	json_object_set_new(tree, "password", json_string(password));
	json_object_set_new(tree, "returnSecureToken", json_true());
	
	char* post_fields __free__ = json_dumps(tree, JSON_COMPACT);
	
	if (post_fields == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	buffer_t string __buffer_free__ = {0};
	
	const char* const headers[][2] = {
		{HTTP_HEADER_CONTENT_TYPE, CONTENT_TYPE_JSON}
	};
	
	struct curl_slist* list __curl_slist_free_all__ = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char* const* const header = headers[index];
		
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, post_fields);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, KIWIFY_LOGIN_ENDPOINT);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* subtree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(subtree, "idToken");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* access_token = json_string_value(obj);
	
	obj = json_object_get(subtree, "displayName");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const full_name = json_string_value(obj);
	
	credentials->username = malloc(strlen(full_name) + 1);
	
	if (credentials->username == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->username, full_name);
	
	credentials->access_token = malloc(strlen(access_token) + 1);
	
	if (credentials->access_token == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->access_token, access_token);
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	
	return UERR_SUCCESS;
	
}

int kiwify_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	buffer_t string __buffer_free__ = {0};
	
	const char* const headers[][2] = {
		{HTTP_HEADER_AUTHORIZATION, authorization}
	};
	
	struct curl_slist* list __curl_slist_free_all__ = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char* const* const header = headers[index];
		
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
	
	CURLU* cu __curl_url_cleanup__ = curl_url();
	
	if (cu == NULL) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, KIWIFY_COURSES_ENDPOINT, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	
	for (size_t index = 1; 1; index++) {
		char page_number[intlen(index) + 1];
		snprintf(page_number, sizeof(page_number), "%zu", index);
		
		struct Query query __query_free__ = {0};
		
		query_add_parameter(&query, "page", page_number);
		
		char* query_fields __free__ = NULL;
		const int code = query_stringify(query, &query_fields);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		char* url __curl_free__ = NULL;
		
		if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
			return UERR_CURLU_FAILURE;
		}
		
		curl_easy_setopt(curl_easy, CURLOPT_URL, url);
		
		buffer_t string __buffer_free__ = {0};
		
		curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
		
		switch (curl_easy_perform_retry(curl_easy)) {
			case CURLE_OK:
				break;
			case CURLE_HTTP_RETURNED_ERROR:
				return UERR_PROVIDER_SESSION_EXPIRED;
			default:
				return UERR_CURL_FAILURE;
		}
		
		json_auto_t* tree = json_loads(string.s, 0, NULL);
		
		if (tree == NULL) {
			return UERR_JSON_CANNOT_PARSE;
		}
		
		const json_t* const courses = json_object_get(tree, "courses");
		
		if (courses == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(courses)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const size_t total_items = json_array_size(courses);
		
		if (total_items < 1) {
			break;
		}
		
		if (index == 1) {
			const json_t* obj = json_object_get(tree, "count");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			 if (!json_is_integer(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_int_t total = json_integer_value(obj);
			
			const size_t size = resources->size + sizeof(struct Resource) * (size_t) total;
			struct Resource* items = realloc(resources->items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			resources->size = size;
			resources->items = items;
		}
		
		size_t index = 0;
		const json_t* item = NULL;
		
		json_array_foreach(courses, index, item) {
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
			
			const int value = hashs(id);
			
			char hash[intlen(value) + 1];
			snprintf(hash, sizeof(hash), "%i", value);
			
			obj = json_object_get(item, "name");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			char* const name = (char*) json_string_value(obj);
			strip(name);
			
			struct Resource resource = {
				.id = malloc(strlen(id) + 1),
				.name = malloc(strlen(name) + 1),
				.dirname = malloc(strlen(name) + 1),
				.short_dirname = malloc(strlen(hash) + 1),
				.qualification = {0},
				.url = malloc(strlen(KIWIFY_COURSE_HOMEPAGE) + strlen(SLASH) + strlen(id) + 1)
			};
			
			if (resource.id == NULL || resource.name == NULL || resource.dirname == NULL || resource.short_dirname == NULL || resource.url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(resource.id, id);
			strcpy(resource.name, name);
			
			strcpy(resource.dirname, name);
			normalize_directory(resource.dirname);
			
			strcpy(resource.short_dirname, hash);
			
			strcpy(resource.url, KIWIFY_COURSE_HOMEPAGE);
			strcat(resource.url, SLASH);
			strcat(resource.url, id);
			
			resources->items[resources->offset++] = resource;
		}
		
		const json_t* obj = json_object_get(tree, "count");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		 if (!json_is_integer(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_int_t count = json_integer_value(obj);
		
		obj = json_object_get(tree, "page_size");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		 if (!json_is_integer(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_int_t page_size = json_integer_value(obj);
		
		if (count < page_size) {
			break;
		}
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int kiwify_get_modules(
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
	
	struct curl_slist* list __curl_slist_free_all__ = NULL;
	
	for (size_t index = 0; index < sizeof(headers) / sizeof(*headers); index++) {
		const char* const* const header = headers[index];
		
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
	
	char url[strlen(KIWIFY_COURSES_ENDPOINT) + strlen(SLASH) + strlen(resource->id) + 1];
	strcpy(url, KIWIFY_COURSES_ENDPOINT);
	strcat(url, SLASH);
	strcat(url, resource->id);
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "course");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "modules");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const size_t array_size = json_array_size(obj);
	
	resource->modules.size = sizeof(*resource->modules.items) * array_size;
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
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const id = json_string_value(obj);
		
		const int value = hashs(id);
			
		char hash[intlen(value) + 1];
		snprintf(hash, sizeof(hash), "%i", value);
			
		obj = json_object_get(item, "name");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
			
		char* const name = (char*) json_string_value(obj);
		strip(name);
		
		obj = json_object_get(item, "active");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_boolean(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const int is_locked = !json_boolean_value(obj);
		
		struct Module module = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1),
			.dirname = malloc(strlen(name) + 1),
			.short_dirname = malloc(strlen(hash) + 1),
			.is_locked = is_locked
		};
		
		if (module.id == NULL || module.name == NULL || module.dirname == NULL || module.short_dirname == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(module.id, id);
		strcpy(module.name, name);
		
		strcpy(module.dirname, name);
		normalize_directory(module.dirname);
		
		strcpy(module.short_dirname, hash);
		
		obj = json_object_get(item, "lessons");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		module.pages.size = sizeof(*module.pages.items) * json_array_size(obj);
		module.pages.items = malloc(module.pages.size);
		
		if (module.pages.items == NULL) {
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
			
			const int value = hashs(id);
				
			char hash[intlen(value) + 1];
			snprintf(hash, sizeof(hash), "%i", value);
			
			obj = json_object_get(item, "title");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			char* const name = (char*) json_string_value(obj);
			strip(name);
			
			obj = json_object_get(item, "content");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			const char* content = NULL;
			
			if (json_is_string(obj)) {
				content = json_string_value(obj);
			} else if (!json_is_null(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			obj = json_object_get(item, "locked");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_boolean(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const int locked = json_boolean_value(obj);
			
			obj = json_object_get(item, "expired");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_boolean(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const int expired = json_boolean_value(obj);
			
			const int is_locked = (locked || expired);
			
			struct Page page = {
				.id = malloc(strlen(id) + 1),
				.name = malloc(strlen(name) + 1),
				.dirname = malloc(strlen(name) + 1),
				.short_dirname = malloc(strlen(hash) + 1),
				.is_locked = is_locked
			};
			
			if (page.id == NULL || page.name == NULL || page.dirname == NULL || page.short_dirname == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(page.id, id);
			strcpy(page.name, name);
			
			strcpy(page.dirname, name);
			normalize_directory(page.dirname);
			
			strcpy(page.short_dirname, hash);
			
			if (content != NULL) {
				page.document.id = malloc(strlen(id) + 1);
				page.document.filename = malloc(strlen(name) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
				page.document.short_filename = malloc(strlen(hash) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
				page.document.content = malloc(strlen(content) + 1);
				
				if (page.document.id == NULL || page.document.filename == NULL || page.document.short_filename == NULL || page.document.content == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(page.document.id, id);
				
				strcpy(page.document.filename, name);
				strcat(page.document.filename, DOT);
				strcat(page.document.filename, HTML_FILE_EXTENSION);
				normalize_directory(page.document.filename);
				
				strcpy(page.document.short_filename, hash);
				strcat(page.document.short_filename, DOT);
				strcat(page.document.short_filename, HTML_FILE_EXTENSION);
				
				strcpy(page.document.content, content);
			}
			
			struct Media media = {0};
			
			const json_t* const video = json_object_get(item, "video");
			
			if (video != NULL) {
				if (!json_is_object(video)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				obj = json_object_get(video, "stream_link");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const stream_link = json_string_value(obj);
				
				if (memcmp(stream_link, VIMEO_URL_PATTERN, strlen(VIMEO_URL_PATTERN)) != 0) {
					return UERR_STRSTR_FAILURE;
				}
				
				obj = json_object_get(video, "external_id");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const external_id = json_string_value(obj);
				
				char url[strlen(VIMEO_URL_PATTERN) + strlen(SLASH) + strlen(VIDEO) + strlen(SLASH) + strlen(external_id) + 1];
				strcpy(url, VIMEO_URL_PATTERN);
				strcat(url, SLASH);
				strcat(url, VIDEO);
				strcat(url, SLASH);
				strcat(url, external_id);
				
				printf("+ A mídia localizada em '%s' aponta para uma fonte externa, verificando se é possível processá-la\r\n", url);
				
				const int code = vimeo_parse(url, resource, &page, &media, resource->url);
				
				if (!(code == UERR_SUCCESS || code == UERR_NO_STREAMS_AVAILABLE)) {
					return code;
				}
			}
			
			if (media.video.url != NULL) {
				page.medias.size = sizeof(*page.medias.items) * 1;
				page.medias.items = malloc(page.medias.size);
				
				if (page.medias.items == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				page.medias.items[page.medias.offset++] = media;
			}
			
			obj = json_object_get(item, "files");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_array(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const size_t total_items = json_array_size(obj);
			
			if (total_items > 0) {
				page.attachments.size = sizeof(*page.attachments.items) * total_items;
				page.attachments.items = malloc(page.attachments.size);
				
				if (page.attachments.items == NULL) {
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
					
					const char* const filename = json_string_value(obj);
					
					obj = json_object_get(item, "url");
					
					if (obj == NULL) {
						return UERR_JSON_MISSING_REQUIRED_KEY;
					}
					
					if (!json_is_string(obj)) {
						return UERR_JSON_NON_MATCHING_TYPE;
					}
					
					const char* const download_url = json_string_value(obj);
					
					const int value = hashs(id);
					
					char hash[intlen(value) + 1];
					snprintf(hash, sizeof(hash), "%i", value);
					
					const char* const file_extension = get_file_extension(filename);
					
					struct Attachment attachment = {
						.id = malloc(strlen(id) + 1),
						.filename = malloc(strlen(filename) + 1),
						.short_filename = malloc(strlen(hash) + strlen(DOT) + strlen(file_extension)  + 1),
						.url = malloc(strlen(download_url) + 1),
					};
					
					if (attachment.id == NULL || attachment.filename == NULL || attachment.short_filename == NULL || attachment.url == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					strcpy(attachment.id, id);
					strcpy(attachment.filename, filename);
					
					strcpy(attachment.short_filename, hash);
					strcat(attachment.short_filename, DOT);
					strcat(attachment.short_filename, file_extension);
					
					normalize_filename(attachment.filename);
					
					strcpy(attachment.url, download_url);
					
					page.attachments.items[page.attachments.offset++] = attachment;
				}
			}
			
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

int kiwify_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	(void) credentials;
	(void) resource;
	(void) module;
	
	return UERR_NOT_IMPLEMENTED;
	
}

int kiwify_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	(void) credentials;
	(void) resource;
	(void) page;
	
	return UERR_NOT_IMPLEMENTED;
	
}
