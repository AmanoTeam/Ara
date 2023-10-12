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
#include "vimeo.h"
#include "youtube.h"
#include "loja_concurseiro.h"

static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";

static const char HTTP_AUTHENTICATION_BEARER[] = "Bearer";

#define LOJA_CONCURSEIRO_HOMEPAGE "https://www.lojadoconcurseiro.com.br"

static const char LOJA_CONCURSEIRO_API_ENDPOINT[] = "https://www.globalead.com.br/app/api/";

static const char LOJA_CONCURSEIRO_COURSE_HOMEPAGE[] = 
	LOJA_CONCURSEIRO_HOMEPAGE
	"/curso/concurso";

static const char LOJA_CONCURSEIRO_DOWNLOAD_TOKEN[] = 
	LOJA_CONCURSEIRO_HOMEPAGE
	"/portal/arquivos/disciplina";

static const char LOJA_CONCURSEIRO_ACCESS_KEY[] = "21YcnS0zUtQH";

static const char LOJA_CONCURSEIRO_ATTACHMENT_TOKEN[] = "302c723b348f75072df5af88784a916f";

static const char AULAS[] = "aulas";

static const char VIMEO_VIDEO_HOMEPAGE[] = "https://player.vimeo.com/video";
static const char YOUTUBE_VIDEO_HOMEPAGE[] = "https://www.youtube.com/embed";

#define PLAYER_VIMEO 1
#define PLAYER_YOUTUBE 2

int loja_concurseiro_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	char* user __curl_free__ = curl_easy_escape(NULL, username, 0);
	
	if (user == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char* pass __curl_free__ = curl_easy_escape(NULL, password, 0);
	
	if (pass == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char authorization[strlen(HTTP_AUTHENTICATION_BEARER) + strlen(SPACE) + strlen(LOJA_CONCURSEIRO_ACCESS_KEY) + 1];
	strcpy(authorization, HTTP_AUTHENTICATION_BEARER);
	strcat(authorization, SPACE);
	strcat(authorization, LOJA_CONCURSEIRO_ACCESS_KEY);
	
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
	
	struct Query query __query_free__ = {0};
	
	query_add_parameter(&query, "model", "Usuario");
	query_add_parameter(&query, "method", "login");
	
	char* query_fields __free__ = NULL;
	int code = query_stringify(query, &query_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU* cu __curl_url_cleanup__ = curl_url();
	
	if (cu == NULL) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, LOJA_CONCURSEIRO_API_ENDPOINT, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	char* url __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	query_free(&query);
	
	query_add_parameter(&query, "plataforma_id", "2");
	query_add_parameter(&query, "login", user);
	query_add_parameter(&query, "senha", pass);
	
	char* post_fields __free__ = NULL;
	code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, post_fields);
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
	
	const json_t* data = json_object_get(tree, "usuario");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* obj = json_object_get(data, "token_app");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* access_token = json_string_value(obj);
	
	obj = json_object_get(data, "nome");
	
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

int loja_concurseiro_get_resources(
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
	
	struct Query query __query_free__ = {0};
	
	query_add_parameter(&query, "model", "Curd");
	query_add_parameter(&query, "method", "findCurds");
	
	char* query_fields __free__ = NULL;
	int code = query_stringify(query, &query_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU* cu __curl_url_cleanup__ = curl_url();
	
	if (cu == NULL) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, LOJA_CONCURSEIRO_API_ENDPOINT, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	char* url __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
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
	
	const json_t* data = json_object_get(tree, "curds");
	
	if (data == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t total = json_array_size(data);
	
	resources->size = sizeof(*resources->items) * (size_t) total;
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
		
		const json_t* obj = json_object_get(item, "id");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const id = json_string_value(obj);
		
		obj = json_object_get(item, "nome");
		
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
			.short_dirname = malloc(strlen(id) + 1),
			.url = malloc(strlen(LOJA_CONCURSEIRO_COURSE_HOMEPAGE) + strlen(SLASH) + strlen(id) + strlen(SLASH) + strlen(AULAS) + 1)
		};
		
		if (resource.id == NULL || resource.name == NULL || resource.dirname == NULL || resource.short_dirname == NULL || resource.url == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(resource.id, id);
		strcpy(resource.name, name);
		
		strcpy(resource.dirname, name);
		normalize_directory(resource.dirname);
		
		strcpy(resource.short_dirname, id);
		
		strcpy(resource.url, LOJA_CONCURSEIRO_COURSE_HOMEPAGE);
		strcat(resource.url, SLASH);
		strcat(resource.url, id);
		strcat(resource.url, SLASH);
		strcat(resource.url, AULAS);
		
		resources->items[resources->offset++] = resource;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int loja_concurseiro_get_modules(
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
	
	struct Query query __query_free__ = {0};
	
	query_add_parameter(&query, "model", "Curd");
	query_add_parameter(&query, "method", "findDisciplinasByCurso");
	
	char* query_fields __free__ = NULL;
	int code = query_stringify(query, &query_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU* cu __curl_url_cleanup__ = curl_url();
	
	if (cu == NULL) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, LOJA_CONCURSEIRO_API_ENDPOINT, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	char* url __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	query_free(&query);
	
	query_add_parameter(&query, "curso_id", resource->id);
	
	char* post_fields __free__ = NULL;
	code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, post_fields);
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
	
	if (!json_is_array(tree)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t total = json_array_size(tree);
	
	resource->modules.size = sizeof(*resource->modules.items) * total;
	resource->modules.items = malloc(resource->modules.size);
	
	if (resource->modules.items == NULL) {
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
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const id = json_string_value(obj);
		
		obj = json_object_get(item, "nome");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		char* const name = (char*) json_string_value(obj);
		strip(name);
		
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
		
		resource->modules.items[resource->modules.offset++] = module;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int loja_concurseiro_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
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
	
	struct Query query __query_free__ = {0};
	
	query_add_parameter(&query, "model", "Apostila");
	query_add_parameter(&query, "method", "findApostilas");
	
	char* query_fields __free__ = NULL;
	int code = query_stringify(query, &query_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU* cu __curl_url_cleanup__ = curl_url();
	
	if (cu == NULL) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_URL, LOJA_CONCURSEIRO_API_ENDPOINT, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	char* url __curl_free__ = NULL;
	
	if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	query_free(&query);
	
	query_add_parameter(&query, "disciplina_id", module->id);
	
	char* post_fields __free__ = NULL;
	code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, post_fields);
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
	
	if (!json_is_array(tree)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t total_attachments = json_array_size(tree);
	
	if (total_attachments > 0) {
		module->attachments.size = sizeof(*module->attachments.items) * (size_t) total_attachments;
		module->attachments.items = malloc(module->attachments.size);
		
		if (module->attachments.items == NULL) {
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
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const id = json_string_value(obj);
			
			obj = json_object_get(item, "nome");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			obj = json_object_get(item, "nome_file");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const filename = json_string_value(obj);
			
			struct Query query __query_free__ = {0};
			
			query_add_parameter(&query, "ac_token", LOJA_CONCURSEIRO_ATTACHMENT_TOKEN);
			
			char* query_fields __free__ = NULL;
			int code = query_stringify(query, &query_fields);
			
			if (code != UERR_SUCCESS) {
				return code;
			}
			
			char* url = malloc(strlen(LOJA_CONCURSEIRO_DOWNLOAD_TOKEN) + strlen(SLASH) + strlen(name) + strlen(QUESTION) + strlen(query_fields) + 1);
			
			if (url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
		
			strcpy(url, LOJA_CONCURSEIRO_DOWNLOAD_TOKEN);
			strcat(url, SLASH);
			strcat(url, name);
			strcat(url, QUESTION);
			strcat(url, query_fields);
			
			char* const file_extension = get_file_extension(name);
			
			struct Attachment attachment = {
				.id = malloc(strlen(id) + 1),
				.filename = malloc(strlen(filename) + 1),
				.short_filename = malloc(strlen(id) + strlen(DOT) + (file_extension == NULL ? strlen(PDF_FILE_EXTENSION) : strlen(file_extension)) + 1),
				.url = url
			};
			
			if (attachment.id == NULL || attachment.filename == NULL || attachment.short_filename == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(attachment.id, id);
			
			strcpy(attachment.filename, filename);
			
			strcpy(attachment.short_filename, id);
			strcat(attachment.short_filename, DOT);
			strcat(attachment.short_filename, (file_extension == NULL ? PDF_FILE_EXTENSION : file_extension));
			
			normalize_filename(attachment.filename);
			
			module->attachments.items[module->attachments.offset++] = attachment;
		}
	}
	
	query_free(&query);
	free(query_fields);
	
	query_add_parameter(&query, "model", "Video");
	query_add_parameter(&query, "method", "findVideos");
	
	code = query_stringify(query, &query_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	curl_free(url);
	
	if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
		return UERR_CURLU_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
	buffer_free(&string);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* subtree = json_loads(string.s, 0, NULL);
	
	if (subtree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	if (!json_is_array(subtree)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_int_t total_videos = json_array_size(subtree);
	
	if (total_videos > 0) {
		module->pages.size = sizeof(*module->pages.items) * (size_t) total_videos;
		module->pages.items = malloc(module->pages.size);
		
		if (module->pages.items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		size_t index = 0;
		const json_t* item = NULL;
		
		json_array_foreach(subtree, index, item) {
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
			
			obj = json_object_get(item, "titulo");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			obj = json_object_get(item, "idvimeo");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* video_id = json_string_value(obj);
			const int player_type = (*video_id == '\0' ? PLAYER_YOUTUBE : PLAYER_VIMEO);
			
			if (player_type == PLAYER_YOUTUBE) {
				const json_t* obj = json_object_get(item, "caminho");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				video_id = json_string_value(obj);
			}
			
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
			
			if (page.medias.items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			struct Media media = {
				.video = {
					.url = malloc(strlen(player_type == PLAYER_VIMEO ? VIMEO_VIDEO_HOMEPAGE : YOUTUBE_VIDEO_HOMEPAGE) + strlen(SLASH) + strlen(video_id) + 1)
				}
			};
			
			if (media.video.url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(media.video.url, player_type == PLAYER_VIMEO ? VIMEO_VIDEO_HOMEPAGE : YOUTUBE_VIDEO_HOMEPAGE);
			strcat(media.video.url, SLASH);
			strcat(media.video.url, video_id);
			
			page.medias.items[page.medias.offset++] = media;
			module->pages.items[module->pages.offset++] = page;
		}
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_POSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	return UERR_SUCCESS;
	
}

int loja_concurseiro_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	(void) credentials;
	
	struct Media* const media = &page->medias.items[0];
	char* url __free__ = media->video.url;
	
	media->video.url = NULL;
	
	printf("+ A mídia localizada em '%s' aponta para uma fonte externa, verificando se é possível processá-la\r\n", url);
	
	if (vimeo_matches(url)) {
		const int code = vimeo_parse(url, resource, page, media, resource->url);
		
		if (!(code == UERR_SUCCESS || code == UERR_NO_STREAMS_AVAILABLE)) {
			return code;
		}
	} else if (youtube_matches(url)) {
		const int code = youtube_parse(url, resource, page, media, NULL);
		
		if (!(code == UERR_SUCCESS || code == UERR_NO_STREAMS_AVAILABLE)) {
			return code;
		}
	}
	
	if (media->video.url == NULL) {
		fprintf(stderr, "- A URL é inválida ou não foi reconhecida. Por favor, reporte-a ao desenvolvedor.\r\n");
	}
	
	return UERR_SUCCESS;
	
}
