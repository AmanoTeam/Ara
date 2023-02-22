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

#define ESTRATEGIA_CONCURSOS_API_ENDPOINT "https://api.estrategiaconcursos.com.br"
#define ESTRATEGIA_CONCURSOS_ACCOUNTS_API_ENDPOINT "https://api.accounts.estrategia.com"
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
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
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
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	
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