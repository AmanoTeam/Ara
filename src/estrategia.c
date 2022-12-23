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
#include "utils.h"
#include "symbols.h"
#include "m3u8.h"
#include "vimeo.h"
#include "youtube.h"
#include "curl.h"
#include "estrategia.h"
#include "html.h"
#include "ttidy.h"

static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";
static const char HTTP_HEADER_CONTENT_TYPE[] = "Content-Type";

static const char HTTP_AUTHENTICATION_BEARER[] = "Bearer";

static const char CONTENT_TYPE_JSON[] = "application/json";

#define ESTRATEGIA_API_PREFIX "https://api.estrategiaconcursos.com.br"
#define ESTRATEGIA_ACCOUNTS_API_PREFIX "https://api.accounts.estrategia.com"
#define ESTRATEGIA_HOMEPAGE_PREFIX "https://www.estrategiaconcursos.com.br"

static const char ESTRATEGIA_LOGIN_ENDPOINT[] = 
	ESTRATEGIA_ACCOUNTS_API_PREFIX
	"/auth/login";

static const char ESTRATEGIA_LOGIN2_ENDPOINT[] = 
	ESTRATEGIA_HOMEPAGE_PREFIX
	"/accounts/login";

static const char ESTRATEGIA_TOKEN_ENDPOINT[] = 
	ESTRATEGIA_HOMEPAGE_PREFIX
	"/oauth/token";

static const char ESTRATEGIA_COURSE_ENDPOINT[] = 
	ESTRATEGIA_API_PREFIX
	"/api/aluno/curso";

static const char ESTRATEGIA_LESSON_ENDPOINT[] = 
	ESTRATEGIA_API_PREFIX
	"/api/aluno/aula";

int estrategia_authorize(
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
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_LOGIN_ENDPOINT);
	
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
	
	struct Query query __attribute__((__cleanup__(query_free))) = {0};
	
	add_parameter(&query, "access_token", access_token);
	
	free(post_fields);
	post_fields = NULL;
	
	const int code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, "");
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_LOGIN2_ENDPOINT);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_discard_body_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	string_free(&string);
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_TOKEN_ENDPOINT);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
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
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	
	return UERR_SUCCESS;
	
}

int estrategia_get_resources(
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
	curl_easy_setopt(curl_easy, CURLOPT_URL, ESTRATEGIA_COURSE_ENDPOINT);
	
	const CURLcode code = curl_easy_perform(curl_easy);
	
	if (code == CURLE_HTTP_RETURNED_ERROR) {
		return UERR_HOTMART_SESSION_EXPIRED;
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
	
	if (!json_is_object(data)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const json_t* obj = json_object_get(data, "concursos");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	size_t index = 0;
	const json_t* item = NULL;
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "cursos");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t subindex = 0;
		const json_t* subitem = NULL;
		const size_t array_size = json_array_size(obj);
		
		const size_t size = resources->size + sizeof(struct Resource) * array_size;
		struct Resource* items = realloc(resources->items, size);
		
		if (items == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		resources->size = size;
		resources->items = items;
		
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
			
			obj = json_object_get(subitem, "nome");
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			struct Resource resource = {
				.id = malloc(strlen(sid) + 1),
				.name = malloc(strlen(name) + 1)
			};
			
			if (resource.id == NULL || resource.name == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(resource.id, sid);
			strcpy(resource.name, name);
			
			resources->items[resources->offset++] = resource;
		}
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int estrategia_get_modules(
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
	
	char url[strlen(ESTRATEGIA_COURSE_ENDPOINT) + strlen(SLASH) + strlen(resource->id) + 1];
	strcpy(url, ESTRATEGIA_COURSE_ENDPOINT);
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
	
	const json_t* obj = json_object_get(tree, "data");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_object(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	obj = json_object_get(obj, "aulas");
	
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
		
		obj = json_object_get(item, "nome");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
			
		const char* const name = json_string_value(obj);
		
		obj = json_object_get(item, "is_disponivel");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_boolean(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const int is_locked = !json_boolean_value(obj);
		
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
			
		struct String string __attribute__((__cleanup__(string_free))) = {0};
		
		curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
		
		char url[strlen(ESTRATEGIA_LESSON_ENDPOINT) + strlen(SLASH) + strlen(sid) + 1];
		strcpy(url, ESTRATEGIA_LESSON_ENDPOINT);
		strcat(url, SLASH);
		strcat(url, sid);
		
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
		
		const char* const keys[] = {
			"pdf_grifado",
			"pdf_simplificado",
			"pdf"
		};
		
		module.attachments.size = sizeof(struct Attachment) * (sizeof(keys) / sizeof(*keys));
		module.attachments.items = malloc(module.attachments.size);
			
		for (size_t index = 0; index < (sizeof(keys) / sizeof(*keys)); index++) {
			const char* const key = keys[index];
			
			const json_t* obj = json_object_get(data, key);
			
			if (obj == NULL) {
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (json_is_string(obj)) {
				const char* const url = json_string_value(obj);
				
				const char* name = NULL;
				
				if (strcmp(key, "pdf_grifado") == 0) {
					name = "Marcação dos aprovados";
				} else if (strcmp(key, "pdf_simplificado") == 0) {
					name = "Versão simplificada";
				} else if (strcmp(key, "pdf") == 0) {
					name = "Versão original";
				}
				
				struct Attachment attachment = {
					.filename = malloc(strlen(name) + strlen(DOT) + strlen(PDF_FILE_EXTENSION) + 1),
					.url = malloc(strlen(url) + 1)
				};
				
				if (attachment.filename == NULL || attachment.url == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(attachment.filename, name);
				strcat(attachment.filename, DOT);
				strcat(attachment.filename, PDF_FILE_EXTENSION);
				
				normalize_filename(attachment.filename);
				
				strcpy(attachment.url, url);
				
				module.attachments.items[module.attachments.offset++] = attachment;
			} else if (!json_is_null(obj)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
		}
		
		obj = json_object_get(data, "videos");
		
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
			
			obj = json_object_get(subitem, "titulo");
			
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
			
			const char* const keys[] = {
				"resumo",
				"slide",
				"mapa_mental",
				"audio"
			};
			
			page.attachments.size = sizeof(struct Attachment) * (sizeof(keys) / sizeof(*keys));
			page.attachments.items = malloc(page.attachments.size);
			
			if (page.attachments.items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			for (size_t index = 0; index < (sizeof(keys) / sizeof(*keys)); index++) {
				const char* const key = keys[index];
				
				const json_t* obj = json_object_get(subitem, key);
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (json_is_string(obj)) {
					const char* const url = json_string_value(obj);
					
					const char* name = NULL;
					
					if (strcmp(key, "resumo") == 0) {
						name = "Resumo";
					} else if (strcmp(key, "slide") == 0) {
						name = "Slide";
					} else if (strcmp(key, "mapa_mental") == 0) {
						name = "Mapa mental";
					} else if (strcmp(key, "audio") == 0) {
						name = "Versão em áudio";
					}
					
					const char* const file_extension = (strcmp(key, "audio") == 0) ? MP3_FILE_EXTENSION : PDF_FILE_EXTENSION;
					
					struct Attachment attachment = {
						.filename = malloc(strlen(name) + strlen(DOT) + strlen(file_extension) + 1),
						.url = malloc(strlen(url) + 1)
					};
					
					if (attachment.filename == NULL || attachment.url == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					strcpy(attachment.filename, name);
					strcat(attachment.filename, DOT);
					strcat(attachment.filename, file_extension);
					
					normalize_filename(attachment.filename);
					
					strcpy(attachment.url, url);
					
					page.attachments.items[page.attachments.offset++] = attachment;
				} else if (!json_is_null(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
			}
			
			obj = json_object_get(subitem, "resolucoes");
			
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
			
			json_object_foreach((json_t*) obj, key, value) {
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
				return UERR_FSTREAM_FAILURE;
			}
			
			page.medias.size = sizeof(struct Media) * 1;
			page.medias.items = malloc(page.medias.size);
			
			if (page.medias.items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			struct Media media = {
				.type = MEDIA_SINGLE,
				.video = {
					.filename = malloc(strlen(name) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1),
					.url = malloc(strlen(stream_uri) + 1)
				}
			};
			
			if (media.video.filename == NULL || media.video.url == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(media.video.filename, name);
			strcat(media.video.filename, DOT);
			strcat(media.video.filename, MP4_FILE_EXTENSION);
			
			normalize_filename(media.video.filename);
			
			strcpy(media.video.url, stream_uri);
			
			page.medias.items[page.medias.offset++] = media;
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