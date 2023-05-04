#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <jansson.h>

#include "buffer.h"
#include "credentials.h"
#include "cleanup.h"
#include "errors.h"
#include "callbacks.h"
#include "types.h"
#include "stringu.h"
#include "query.h"
#include "symbols.h"
#include "curl.h"
#include "curl_cleanup.h"
#include "query_cleanup.h"
#include "buffer_cleanup.h"
#include "estrategia.h"

static const char HTTP_HEADER_CONTENT_TYPE[] = "Content-Type";

static const char CONTENT_TYPE_JSON[] = "application/json";

#define ESTRATEGIA_ENDPOINT_SUFFIX ".estrategia.com"
#define ESTRATEGIA_API_ENDPOINT "https://api" ESTRATEGIA_ENDPOINT_SUFFIX

#define ESTRATEGIA_CONCURSOS_API_ENDPOINT "https://api.estrategiaconcursos.com.br"
#define ESTRATEGIA_CONCURSOS_ACCOUNTS_API_ENDPOINT "https://api.accounts" ESTRATEGIA_ENDPOINT_SUFFIX
#define ESTRATEGIA_CONCURSOS_HOMEPAGE_ENDPOINT "https://www.estrategiaconcursos.com.br"

static const char ESTRATEGIA_CONCURSOS_LOGIN_ENDPOINT[] = 
	ESTRATEGIA_CONCURSOS_ACCOUNTS_API_ENDPOINT
	"/auth/login";

static const char ESTRATEGIA_CONCURSOS_LOGIN2_ENDPOINT[] = 
	ESTRATEGIA_CONCURSOS_HOMEPAGE_ENDPOINT
	"/accounts/login";

static const char ESTRATEGIA_CONCURSOS_TOKEN_ENDPOINT[] = 
	ESTRATEGIA_CONCURSOS_HOMEPAGE_ENDPOINT
	"/oauth/token";

static const char ESTRATEGIA_COURSES_ENDPOINT[] = 
	ESTRATEGIA_API_ENDPOINT
	"/v3/courses/catalog?page=1&perPage=1000";

static const char ESTRATEGIA_COURSE_ENDPOINT[] = 
	ESTRATEGIA_API_ENDPOINT
	"/v3/courses/slug";

static const char MEUS_CURSOS[] = "meus-cursos";

static const char* ESTRATEGIA_VERTICAL = NULL;

void estrategia_set_vertical(const char* const name) {
	ESTRATEGIA_VERTICAL = name;
}

int estrategia_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	json_auto_t* tree = json_object();
	
	json_object_set_new(tree, "email", json_string(username));
	json_object_set_new(tree, "password", json_string(password));
	
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
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_CONCURSOS_LOGIN_ENDPOINT);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, "");
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
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
	
	obj = json_object_get(data, "full_name");
	
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
	
	struct Query query __query_free__ = {0};
	
	add_parameter(&query, "access_token", access_token);
	
	free(post_fields);
	post_fields = NULL;
	
	const int code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_CONCURSOS_LOGIN2_ENDPOINT);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_discard_body_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, post_fields);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	buffer_free(&string);
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_CONCURSOS_TOKEN_ENDPOINT);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_decref(subtree);
	
	subtree = json_loads(string.s, 0, NULL);
	
	if (subtree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	obj = json_object_get(subtree, "access_token");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	access_token = json_string_value(obj);
	
	credentials->access_token = malloc(strlen(access_token) + 1);
	
	if (credentials->access_token == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->access_token, access_token);
	
	const int value = hashs(credentials->username);
	
	char hash[intlen(value) + 1];
	snprintf(hash, sizeof(hash), "%i", value);
	
	credentials->cookie_jar = malloc(strlen(credentials->directory) + strlen(PATH_SEPARATOR) + strlen(hash) + strlen(DOT) + strlen(TXT_FILE_EXTENSION) + 1);
	
	if (credentials->cookie_jar == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->cookie_jar, credentials->directory);
	strcat(credentials->cookie_jar, PATH_SEPARATOR);
	strcat(credentials->cookie_jar, hash);
	strcat(credentials->cookie_jar, DOT);
	strcat(credentials->cookie_jar, TXT_FILE_EXTENSION);
	
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEJAR, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIELIST, "FLUSH");
	
	curl_easy_setopt(curl_easy, CURLOPT_COOKIELIST, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEJAR, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	
	return UERR_SUCCESS;
	
}

int estrategia_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	const char* const headers[][2] = {
		{"X-Vertical", ESTRATEGIA_VERTICAL}
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
		
		struct curl_slist* tmp = curl_slist_append(list, item);
		
		if (tmp == NULL) {
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	buffer_t string __buffer_free__ = {0};
	
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
		
		char* const name = (char*) json_string_value(obj);
		strip(name);
		
		const int value = hashs(id);
		
		char hash[intlen(value) + 1];
		snprintf(hash, sizeof(hash), "%i", value);
		
		struct Resource resource = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1),
			.dirname = malloc(strlen(name) + 1),
			.short_dirname = malloc(strlen(hash) + 1),
			.qualification = {0},
			.url = malloc(strlen(HTTPS_SCHEME) + strlen(ESTRATEGIA_VERTICAL) + strlen(ESTRATEGIA_ENDPOINT_SUFFIX) + strlen(SLASH) + strlen(MEUS_CURSOS) + strlen(SLASH) + strlen(id) + 1)
		};
		
		if (resource.id == NULL || resource.name == NULL || resource.dirname == NULL || resource.short_dirname == NULL || resource.url == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(resource.id, id);
		strcpy(resource.name, name);
		
		strcpy(resource.dirname, name);
		normalize_directory(resource.dirname);
		
		strcpy(resource.short_dirname, hash);
		
		strcpy(resource.url, HTTPS_SCHEME);
		strcat(resource.url, ESTRATEGIA_VERTICAL);
		strcat(resource.url, ESTRATEGIA_ENDPOINT_SUFFIX);
		strcat(resource.url, SLASH);
		strcat(resource.url, MEUS_CURSOS);
		strcat(resource.url, SLASH);
		strcat(resource.url, id);
		
		resources->items[resources->offset++] = resource;
	}
	
	return UERR_SUCCESS;
		
}

int estrategia_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	const char* const headers[][2] = {
		{"X-Vertical", ESTRATEGIA_VERTICAL}
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
		
		struct curl_slist* tmp = curl_slist_append(list, item);
		
		if (tmp == NULL) {
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	buffer_t string __buffer_free__ = {0};
	
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
		
		char* const name = (char*) json_string_value(obj);
		strip(name);
		
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

int estrategia_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	(void) credentials;
	(void) resource;
	(void) module;
	
	return UERR_NOT_IMPLEMENTED;
	
}

int estrategia_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	(void) credentials;
	(void) resource;
	(void) page;
	
	return UERR_NOT_IMPLEMENTED;
	
}