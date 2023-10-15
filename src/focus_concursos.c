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
#include "uri.h"
#include "focus_concursos.h"

static const char HTTP_HEADER_CONTENT_TYPE[] = "Content-Type";

static const char CONTENT_TYPE_JSON[] = "application/json";

static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";

static const char HTTP_AUTHENTICATION_BEARER[] = "Bearer";

#define FOCUS_CONCURSOS_API_ENDPOINT "https://mobile.focusconcursos.com.br"

static const char FOCUS_CONCURSOS_COURSE_HOMEPAGE[] = "https://lms.focusconcursos.com.br/#/curso";

static const char FOCUS_CONCURSOS_LOGIN_ENDPOINT[] = 
	FOCUS_CONCURSOS_API_ENDPOINT
	"/login";

static const char FOCUS_CONCURSOS_COURSES_ENDPOINT[] = 
	FOCUS_CONCURSOS_API_ENDPOINT
	"/api/person/courses";

static const char FOCUS_CONCURSOS_MEDIA_ENDPOINT[] = 
	FOCUS_CONCURSOS_API_ENDPOINT
	"/media";

int focus_concursos_authorize(
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
	curl_easy_setopt(curl_easy, CURLOPT_URL, FOCUS_CONCURSOS_LOGIN_ENDPOINT);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* subtree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(subtree, "access_token");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* access_token = json_string_value(obj);
	
	obj = json_object_get(subtree, "user");
	
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	return UERR_SUCCESS;
	
}

int focus_concursos_get_resources(
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, FOCUS_CONCURSOS_COURSES_ENDPOINT);
	
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
	
	if (!json_is_array(tree)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t total = json_array_size(tree);
	
	resources->size = sizeof(*resources->items) * (size_t) total;
	resources->items = malloc(resources->size);
	
	if (resources->items == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	size_t index = 0;
	const json_t* item = NULL;
	
	json_array_foreach(tree, index, item) {
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
		
		const json_t* data = json_object_get(item, "product");
		
		if (data == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(data)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		obj = json_object_get(data, "name");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		char* const name = (char*) json_string_value(obj);
		
		data = json_object_get(data, "career");
		
		if (data == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(data)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		obj = json_object_get(data, "name");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const qualification = json_string_value(obj);
		
		const int value = hashs(qualification);
		
		char qualification_id[intlen(value) + 1];
		snprintf(qualification_id, sizeof(qualification_id), "%i", value);
		
		struct Resource resource = {
			.id = malloc(strlen(sid) + 1),
			.name = malloc(strlen(name) + 1),
			.dirname = malloc(strlen(name) + 1),
			.short_dirname = malloc(strlen(sid) + 1),
			.qualification = {
				.id = malloc(strlen(qualification_id) + 1),
				.name = malloc(strlen(qualification) + 1),
				.dirname = malloc(strlen(qualification) + 1),
				.short_dirname = malloc(strlen(qualification_id) + 1)
			},
			.url = malloc(strlen(FOCUS_CONCURSOS_COURSE_HOMEPAGE) + strlen(SLASH) + strlen(sid) + 1)
		};
		
		if (resource.id == NULL || resource.name == NULL || resource.dirname == NULL || resource.short_dirname == NULL || resource.qualification.id == NULL || resource.qualification.name == NULL || resource.qualification.dirname == NULL || resource.qualification.short_dirname == NULL || resource.url == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(resource.id, sid);
		strcpy(resource.name, name);
		
		strcpy(resource.dirname, name);
		normalize_directory(resource.dirname);
		
		strcpy(resource.short_dirname, sid);
		
		strcpy(resource.qualification.id, qualification_id);
		strcpy(resource.qualification.name, qualification);
		
		strcpy(resource.qualification.dirname, qualification);
		normalize_directory(resource.qualification.dirname);
		
		strcpy(resource.qualification.short_dirname, qualification_id);
		
		strcpy(resource.url, FOCUS_CONCURSOS_COURSE_HOMEPAGE);
		strcat(resource.url, SLASH);
		strcat(resource.url, sid);
		
		resources->items[resources->offset++] = resource;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int focus_concursos_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
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
	
	char url[strlen(FOCUS_CONCURSOS_COURSES_ENDPOINT) + strlen(SLASH) + strlen(resource->id) + 1];
	strcpy(url, FOCUS_CONCURSOS_COURSES_ENDPOINT);
	strcat(url, SLASH);
	strcat(url, resource->id);
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	if (!json_is_object(tree)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* data = json_object_get(tree, "product");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	data = json_object_get(data, "subjects");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t total = json_array_size(data);
	
	resource->modules.size = sizeof(*resource->modules.items) * total;
	resource->modules.items = malloc(resource->modules.size);
	
	if (resource->modules.items == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	size_t index = 0;
	const json_t* item = NULL;
	
	json_array_foreach(data, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "name");
		
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
		
		obj = json_object_get(item, "lessons");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
	
		const json_int_t total = json_array_size(obj);
		
		module.pages.size = sizeof(*module.pages.items) * total;
		module.pages.items = malloc(module.pages.size);
		
		if (module.pages.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		size_t subindex = 0;
		const json_t* subitem = NULL;
		
		json_array_foreach(obj, subindex, subitem) {
			if (!json_is_object(subitem)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(subitem, "name");
			
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
			
			struct Page page = {
				.id = malloc(strlen(id) + 1),
				.name = malloc(strlen(name) + 1),
				.dirname = malloc(strlen(name) + 1),
				.short_dirname = malloc(strlen(id) + 1)
			};
			
			if (page.id == NULL || page.name == NULL || page.dirname == NULL || page.short_dirname == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(page.id, id);
			strcpy(page.name, name);
			
			strcpy(page.dirname, name);
			normalize_directory(page.dirname);
			
			strcpy(page.short_dirname, id);
			
			page.medias.size = sizeof(*page.medias.items) * 1;
			page.medias.items = malloc(page.medias.size);
			
			obj = json_object_get(subitem, "providers");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_array(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* data = json_array_get(obj, 0);
			
			if (data == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_object(data)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			obj = json_object_get(data, "url");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const media_id = json_string_value(obj);
			
			obj = json_object_get(data, "name");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const provider_name = json_string_value(obj);
			
			struct Media media = {
				.type = MEDIA_SINGLE,
				.video = {
					.id = malloc(strlen(media_id) + 1),
					.filename = malloc(strlen(name) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1),
					.short_filename = malloc(strlen(media_id) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1),
					.url = malloc(strlen(provider_name) + strlen(SLASH) + strlen(media_id) + 1)
				}
			};
			
			if (media.video.id == NULL || media.video.filename == NULL || media.video.short_filename == NULL || media.video.url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(media.video.id, media_id);
			
			strcpy(media.video.filename, name);
			strcat(media.video.filename, DOT);
			strcat(media.video.filename, MP4_FILE_EXTENSION);
			
			normalize_filename(media.video.filename);
			
			strcpy(media.video.short_filename, media_id);
			strcat(media.video.short_filename, DOT);
			strcat(media.video.short_filename, MP4_FILE_EXTENSION);
			
			strcpy(media.video.url, provider_name);
			strcat(media.video.url, SLASH);
			strcat(media.video.url, media_id);
			
			page.medias.items[page.medias.offset++] = media;
			
			obj = json_object_get(subitem, "attachment");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (json_is_object(obj)) {
				page.attachments.size = sizeof(*page.attachments.items) * 1;
				page.attachments.items = malloc(page.attachments.size);
				
				if (page.attachments.items == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				obj = json_object_get(obj, "url");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const attachment_url = json_string_value(obj);
				
				const size_t size = requote_uri(attachment_url, NULL);
				
				char* requoted_uri = malloc(size);
				
				if (requoted_uri == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				requote_uri(attachment_url, requoted_uri);
				
				const char* file_extension = get_file_extension(requoted_uri);
				
				if (file_extension == NULL) {
					file_extension = PDF_FILE_EXTENSION;
				}
				
				struct Attachment attachment = {
					.id = malloc(strlen(id) + 1),
					.filename = malloc(strlen(name) + strlen(DOT) + strlen(file_extension) + 1),
					.short_filename = malloc(strlen(id) + strlen(DOT) + strlen(file_extension) + 1),
					.url = requoted_uri
				};
				
				if (attachment.id == NULL || attachment.filename == NULL || attachment.short_filename == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(attachment.id, id);
				
				strcpy(attachment.filename, name);
				strcat(attachment.filename, DOT);
				strcat(attachment.filename, file_extension);
				
				strcpy(attachment.short_filename, id);
				strcat(attachment.short_filename, DOT);
				strcat(attachment.short_filename, file_extension);
				
				normalize_filename(attachment.filename);
				
				page.attachments.items[page.attachments.offset++] = attachment;
			} else if (!json_is_null(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			module.pages.items[module.pages.offset++] = page;
		}
		
		resource->modules.items[resource->modules.offset++] = module;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int focus_concursos_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	(void) credentials;
	(void) resource;
	(void) module;
	
	return UERR_NOT_IMPLEMENTED;
	
}

int focus_concursos_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	(void) credentials;
	(void) resource;
	
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
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	
	for (size_t index = 0; index < page->medias.offset; index++) {
		struct Media* const media = &page->medias.items[index];
		
		char* media_url __free__ = media->video.url;
		
		char url[strlen(FOCUS_CONCURSOS_MEDIA_ENDPOINT) + strlen(SLASH) + strlen(media_url) + 1];
		strcpy(url, FOCUS_CONCURSOS_MEDIA_ENDPOINT);
		strcat(url, SLASH);
		strcat(url, media_url);
		
		curl_easy_setopt(curl_easy, CURLOPT_URL, url);
		
		buffer_free(&string);
		
		if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
		
		json_auto_t* tree = json_loads(string.s, 0, NULL);
		
		if (tree == NULL) {
			return UERR_JSON_CANNOT_PARSE;
		}
		
		if (!json_is_array(tree)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		json_int_t last_width = 0;
		const char* stream_uri = NULL;
		
		size_t index = 0;
		const json_t* item = NULL;
		
		json_array_foreach(tree, index, item) {
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
				const json_t* const obj = json_object_get(item, "link");
				
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
		
		media->video.url = malloc(strlen(stream_uri) + 1);
		
		if (media->video.url == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(media->video.url, stream_uri);
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}
