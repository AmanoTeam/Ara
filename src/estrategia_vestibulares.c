#include <stdio.h>

#include <curl/curl.h>
#include <jansson.h>

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
#include "estrategia.h"

#define ESTRATEGIA_API_ENDPOINT "https://api.estrategia.com"
#define ESTRATEGIA_VESTIBULARES_HOMEPAGE "https://vestibulares.estrategia.com"

static const char ESTRATEGIA_COURSES_ENDPOINT[] = 
	ESTRATEGIA_API_ENDPOINT
	"/v3/courses/catalog?page=1&perPage=1000";

static const char ESTRATEGIA_COURSE_ENDPOINT[] = 
	ESTRATEGIA_API_ENDPOINT
	"/v3/courses/slug";

static const char ESTRATEGIA_VESTIBULARES_COURSE_HOMEPAGE[] = 
	ESTRATEGIA_VESTIBULARES_HOMEPAGE
	"/meus-cursos";

static const char* const DEFAULT_HEADERS[][2] = {
	{"X-Vertical", "vestibulares"}
};

int estrategia_vestibulares_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	const int code = estrategia_authorize(username, password, credentials);
	return code;
	
}

int estrategia_vestibulares_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(DEFAULT_HEADERS) / sizeof(*DEFAULT_HEADERS); index++) {
		const char* const* const header = DEFAULT_HEADERS[index];
		
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_COURSES_ENDPOINT);
	
	switch (curl_easy_perform_retry(curl_easy)) {
		case CURLE_OK:
			break;
		case CURLE_HTTP_RETURNED_ERROR:
			return UERR_PROVIDER_SESSION_EXPIRED;
		default:
			return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* const data = json_object_get(tree, "data");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const size_t array_size = json_array_size(data);
	
	resources->size = sizeof(*resources->items) * array_size;
	resources->items = malloc(resources->size);
		
	if (resources->items == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	size_t index = 0;
	const json_t* item = NULL;
	
	json_array_foreach(data, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "slug");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const id = json_string_value(obj);
		
		obj = json_object_get(item, "title");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const name = json_string_value(obj);
		
		const int value = hashs(id);
		
		char hash[intlen(value) + 1];
		snprintf(hash, sizeof(hash), "%i", value);
		
		struct Resource resource = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1),
			.dirname = malloc(strlen(name) + 1),
			.short_dirname = malloc(strlen(hash) + 1),
			.qualification = {0},
			.url = malloc(strlen(ESTRATEGIA_VESTIBULARES_COURSE_HOMEPAGE) + strlen(SLASH) + strlen(id) + 1)
		};
		
		if (resource.id == NULL || resource.name == NULL || resource.dirname == NULL || resource.short_dirname == NULL || resource.url == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(resource.id, id);
		strcpy(resource.name, name);
		
		strcpy(resource.dirname, name);
		normalize_directory(resource.dirname);
		
		strcpy(resource.short_dirname, hash);
		
		strcpy(resource.url, ESTRATEGIA_VESTIBULARES_COURSE_HOMEPAGE);
		strcat(resource.url, SLASH);
		strcat(resource.url, id);
		
		resources->items[resources->offset++] = resource;
	}
	
	return UERR_SUCCESS;
		
}

int estrategia_vestibulares_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
	for (size_t index = 0; index < sizeof(DEFAULT_HEADERS) / sizeof(*DEFAULT_HEADERS); index++) {
		const char* const* const header = DEFAULT_HEADERS[index];
		
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
	
	char url[strlen(ESTRATEGIA_COURSE_ENDPOINT) + strlen(SLASH) + strlen(resource->id) + 1];
	strcpy(url, ESTRATEGIA_COURSE_ENDPOINT);
	strcat(url, SLASH);
	strcat(url, resource->id);
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "data");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "classes");
	
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
		
		const json_t* obj = json_object_get(item, "title");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const name = json_string_value(obj);
		
		const int value = hashs(name);
		
		char id[intlen(value) + 1];
		snprintf(id, sizeof(id), "%i", value);
		
		struct Module module = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1),
			.dirname = malloc(strlen(name) + 1),
			.short_dirname = malloc(strlen(id) + 1)
		};
		
		if (module.id == NULL || module.name == NULL || module.dirname == NULL || module.short_dirname == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(module.id, id);
		strcpy(module.name, name);
		
		strcpy(module.dirname, name);
		normalize_directory(module.dirname);
		
		strcpy(module.short_dirname, id);
		
		obj = json_object_get(item, "videos");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const size_t total_streams = json_array_size(obj);
		
		obj = json_object_get(item, "contents");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const size_t total_attachments = json_array_size(obj) - total_streams;
		
		module.pages.size = sizeof(*module.pages.items) * 1;
		module.pages.items = malloc(module.pages.size);
		
		if (module.pages.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		struct Page page = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1),
			.dirname = malloc(strlen(name) + 1),
			.short_dirname = malloc(strlen(id) + 1),
			.is_locked = 0
		};
		
		if (page.id == NULL || page.name == NULL || page.dirname == NULL || page.short_dirname == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(page.id, id);
		strcpy(page.name, name);
		
		strcpy(page.dirname, name);
		normalize_directory(page.dirname);
		
		strcpy(page.short_dirname, id);
		
		if (total_streams > 0) {
			page.medias.size = sizeof(*page.medias.items) * total_streams;
			page.medias.items = malloc(page.medias.size);
			
			if (page.medias.items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
		}
		
		if (total_attachments > 0) {
			page.attachments.size = sizeof(*page.attachments.items) * total_attachments;
			page.attachments.items = malloc(page.attachments.size);
			
			if (page.attachments.items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
		}
		
		size_t index = 0;
		const json_t* item = NULL;
		
		json_array_foreach(obj, index, item) {
			if (!json_is_object(item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(item, "type");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const type = json_string_value(obj);
			
			obj = json_object_get(item, "name");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			const int value = hashs(name);
			
			char id[intlen(value) + 1];
			snprintf(id, sizeof(id), "%i", value);
		
			if (strcmp(type, "video") == 0) {
				json_t* const obj = json_object_get(item, "resolutions");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_object(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* key = NULL;
				json_t* value = NULL;
				
				int last_width = 0;
				const char* stream_uri = NULL;
				
				json_object_foreach(obj, key, value) {
					const char* const end = strstr(key, "p");
					
					if (end == NULL) {
						return UERR_STRSTR_FAILURE;
					}
					
					const size_t size = (size_t) (end - key);
					
					char val[size];
					memcpy(val, key, size);
					val[size] = '\0';
					
					const int width = atoi(val);
					
					if (last_width < width) {
						last_width = width;
						stream_uri = json_string_value(value);
					}
				}
				
				if (stream_uri == NULL) {
					return UERR_NO_STREAMS_AVAILABLE;
				}
				
				struct Media media = {
					.type = MEDIA_SINGLE,
					.video = {
						.id = malloc(strlen(id) + 1),
						.filename = malloc(strlen(name) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1),
						.short_filename = malloc(strlen(id) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1),
						.url = malloc(strlen(stream_uri) + 1)
					}
				};
				
				if (media.video.id == NULL || media.video.filename == NULL || media.video.short_filename == NULL || media.video.url == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(media.video.id, id);
				
				strcpy(media.video.filename, name);
				strcat(media.video.filename, DOT);
				strcat(media.video.filename, MP4_FILE_EXTENSION);
				
				normalize_filename(media.video.filename);
				
				strcpy(media.video.short_filename, id);
				strcat(media.video.short_filename, DOT);
				strcat(media.video.short_filename, MP4_FILE_EXTENSION);
				
				strcpy(media.video.url, stream_uri);
				
				page.medias.items[page.medias.offset++] = media;
			} else if (strcmp(type, "pdf") == 0) {
				const json_t* obj = json_object_get(item, "data");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const url = json_string_value(obj);
				
				struct Attachment attachment = {
					.id = malloc(strlen(id) + 1),
					.filename = malloc(strlen(name) + strlen(DOT) + strlen(PDF_FILE_EXTENSION) + 1),
					.short_filename = malloc(strlen(id) + strlen(DOT) + strlen(PDF_FILE_EXTENSION) + 1),
					.url = malloc(strlen(url) + 1)
				};
				
				if (attachment.id == NULL || attachment.filename == NULL || attachment.short_filename == NULL || attachment.url == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(attachment.id, id);
				
				strcpy(attachment.filename, name);
				strcat(attachment.filename, DOT);
				strcat(attachment.filename, PDF_FILE_EXTENSION);
				
				strcpy(attachment.short_filename, id);
				strcat(attachment.short_filename, DOT);
				strcat(attachment.short_filename, PDF_FILE_EXTENSION);
				
				normalize_filename(attachment.filename);
				
				strcpy(attachment.url, url);
				
				page.attachments.items[page.attachments.offset++] = attachment;
			}
		}
		
		module.pages.items[module.pages.offset++] = page;
		resource->modules.items[resource->modules.offset++] = module;
	}
	
	return UERR_SUCCESS;
		
}

int estrategia_vestibulares_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	(void) credentials;
	(void) resource;
	(void) module;
	
	return UERR_NOT_IMPLEMENTED;
	
}

int estrategia_vestibulares_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	(void) credentials;
	(void) resource;
	(void) page;
	
	return UERR_NOT_IMPLEMENTED;
	
}