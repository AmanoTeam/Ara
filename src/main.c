#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>

#include <curl/curl.h>
#include <jansson.h>
#include <bearssl.h>

#include "errors.h"
#include "query.h"
#include "callbacks.h"
#include "types.h"
#include "utils.h"
#include "sha256.h"

static const char DOT[] = ".";
static const char SLASH[] = "/";
static const char SPACE[] = " ";
static const char QUOTATION_MARK[] = "\"";

static const char JSON_FILE_EXTENSION[] = "json";
static const char HTTPS_SCHEME[] = "https://";

static const char APP_CONFIG_DIRECTORY[] = ".config";

static const char HTTP_HEADER_AUTHORIZATION[] = "Authorization";
static const char HTTP_HEADER_SEPARATOR[] = ": ";

static const char* const HOSTNAMES[] = {
	"dns.google:443:8.8.8.8",
	"dns.google:443:8.8.4.4"
};

static const char HOTMART_CLUB_SUFFIX[] = ".club.hotmart.com";

#ifdef _WIN32
	static const char PATH_SEPARATOR[] = "\\";
#else
	static const char PATH_SEPARATOR[] = "/";
#endif

#define MAX_INPUT_SIZE 1024

struct Credentials {
	char* access_token;
	char* refresh_token;
	int expires_in;
};

struct Media {
	char* url;
};

struct Medias {
	size_t offset;
	size_t size;
	struct Media* items;
};

struct Attachment {
	char* url;
};

struct Attachments {
	size_t offset;
	size_t size;
	struct Attachment* items;
};

struct Page {
	char* id;
	char* name;
	struct Medias medias;
	struct Attachments attachments;
};

struct Pages {
	size_t offset;
	size_t size;
	struct Page* items;
};

struct Module {
	char* id;
	char* name;
	char* download_location;
	struct Pages pages;
};

struct Modules {
	size_t offset;
	size_t size;
	struct Module* items;
};

struct Resource {
	char* name;
	char* subdomain;
	char* download_location;
	struct Modules modules;
};

struct Resources {
	size_t offset;
	size_t size;
	struct Resource* items;
};

static CURL* curl = NULL;

static int authorize(
	const char* const username,
	const char* const password,
	struct Credentials* credentials
) {
	
	char* user = curl_easy_escape(NULL, username, 0);
	
	if (user == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char* pass = curl_easy_escape(NULL, password, 0);
	
	if (pass == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	struct Query query = {};
	
	add_parameter(&query, "grant_type", "password");
	add_parameter(&query, "username", user);
	add_parameter(&query, "password", pass);
	
	char* post_fields = NULL;
	const int code = query_stringify(query, &post_fields);
	query_free(&query);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, post_fields);
	
	free(post_fields);
	
	struct String string = {0};
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_URL, "https://api.sparkleapp.com.br/oauth/token");
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_t* tree = json_loads(string.s, 0, NULL);
	
	string_free(&string);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "access_token");
	
	if (obj == NULL) {
		json_decref(tree);
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		json_decref(tree);
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const access_token = json_string_value(obj);
	
	obj = json_object_get(tree, "refresh_token");
	
	if (obj == NULL) {
		json_decref(tree);
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		json_decref(tree);
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const refresh_token = json_string_value(obj);
	
	obj = json_object_get(tree, "expires_in");
	
	if (obj == NULL) {
		json_decref(tree);
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_integer(obj)) {
		json_decref(tree);
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const int expires_in = json_integer_value(obj);
	
	credentials->access_token = malloc(strlen(access_token) + 1);
	strcpy(credentials->access_token, access_token);
	
	credentials->refresh_token = malloc(strlen(refresh_token) + 1);
	strcpy(credentials->refresh_token, refresh_token);
	
	credentials->expires_in = expires_in;
	
	json_decref(tree);
	
	return UERR_SUCCESS;
	
}

static int get_resources(
	const struct Credentials* const credentials,
	struct Resources* resources
) {
	
	struct Query query = {};
	
	add_parameter(&query, "token", credentials->access_token);
	
	char* squery = NULL;
	const int code = query_stringify(query, &squery);
	
	query_free(&query);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	CURLU *cu = curl_url();
	curl_url_set(cu, CURLUPART_URL, "https://api-sec-vlc.hotmart.com/security/oauth/check_token", 0);
	curl_url_set(cu, CURLUPART_QUERY, squery, 0);
	
	char* url = NULL;
	curl_url_get(cu, CURLUPART_URL, &url, 0);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	curl_url_cleanup(cu);
	curl_free(url);
	free(squery);
	
	struct String string = {0};
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_t* tree = json_loads(string.s, 0, NULL);
	
	string_free(&string);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "resources");
	
	if (obj == NULL) {
		json_decref(tree);
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		json_decref(tree);
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	char authorization[6 + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, "Bearer");
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	size_t index = 0;
	json_t *item = NULL;
	const size_t array_size = json_array_size(obj);
	
	curl_easy_setopt(curl, CURLOPT_URL, "https://api-club.hotmart.com/hot-club-api/rest/v3/membership");
	
	resources->size = sizeof(struct Resource) * array_size;
	resources->items = malloc(resources->size);
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "resource");
		
		if (obj == NULL) {
			json_decref(tree);
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_object(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		obj = json_object_get(obj, "subdomain");
		
		if (obj == NULL) {
			json_decref(tree);
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const subdomain = json_string_value(obj);
		
		const char* const headers[][2] = {
			{"Authorization", authorization},
			{"Club", subdomain}
		};
		
		struct curl_slist* list = NULL;
		
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
				curl_slist_free_all(list);
				return UERR_CURL_FAILURE;
			}
			
			list = tmp;
		}
		
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		
		if (curl_easy_perform(curl) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
		
		curl_slist_free_all(list);
		
		json_t* tree = json_loads(string.s, 0, NULL);
		
		string_free(&string);
		
		if (tree == NULL) {
			return UERR_JSON_CANNOT_PARSE;
		}
		
		obj = json_object_get(tree, "name");
		
		if (obj == NULL) {
			json_decref(tree);
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const name = json_string_value(obj);
		
		struct Resource resource = {
			.name = malloc(strlen(name) + 1),
			.subdomain = malloc(strlen(subdomain) + 1)
		};
		
		strcpy(resource.name, name);
		strcpy(resource.subdomain, subdomain);
		
		resources->items[resources->offset++] = resource;
		
		json_decref(tree);
	}
	
	return UERR_SUCCESS;
	
}

static int get_modules(
	const struct Credentials* const credentials,
	struct Resource* resource
) {
	
	char authorization[6 + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, "Bearer");
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	const char* const headers[][2] = {
		{"Authorization", authorization},
		{"Club", resource->subdomain}
	};
	
	struct curl_slist* list = NULL;
	
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
			curl_slist_free_all(list);
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	struct String string = {0};
	
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_URL, "https://api-club.hotmart.com/hot-club-api/rest/v3/navigation");
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_slist_free_all(list);
	
	json_t* tree = json_loads(string.s, 0, NULL);
	
	string_free(&string);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "modules");
	
	if (obj == NULL) {
		json_decref(tree);
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_array(obj)) {
		json_decref(tree);
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	size_t index = 0;
	json_t *item = NULL;
	const size_t array_size = json_array_size(obj);
	
	curl_easy_setopt(curl, CURLOPT_URL, "https://api-club.hotmart.com/hot-club-api/rest/v3/membership");
	
	resource->modules.size = sizeof(struct Module) * array_size;
	resource->modules.items = malloc(resource->modules.size);
	
	json_array_foreach(obj, index, item) {
		if (!json_is_object(item)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const json_t* obj = json_object_get(item, "id");
		
		if (obj == NULL) {
			json_decref(tree);
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const id = json_string_value(obj);
		
		obj = json_object_get(item, "name");
		
		if (obj == NULL) {
			json_decref(tree);
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const name = json_string_value(obj);
		
		struct Module module = {
			.id = malloc(strlen(id) + 1),
			.name = malloc(strlen(name) + 1)
		};
		
		strcpy(module.id, id);
		strcpy(module.name, name);
		
		obj = json_object_get(item, "pages");
		
		if (obj == NULL) {
			json_decref(tree);
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_array(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t page_index = 0;
		json_t *page_item = NULL;
		const size_t array_size = json_array_size(obj);
		
		module.pages.size = sizeof(struct Page) * array_size;
		module.pages.items = malloc(module.pages.size);
		
		json_array_foreach(obj, page_index, page_item) {
			if (!json_is_object(page_item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(page_item, "hash");
			
			if (obj == NULL) {
				json_decref(tree);
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				json_decref(tree);
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const hash = json_string_value(obj);
			
			obj = json_object_get(page_item, "name");
			
			if (obj == NULL) {
				json_decref(tree);
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				json_decref(tree);
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const name = json_string_value(obj);
			
			struct Page page = {
				.id = malloc(strlen(hash) + 1),
				.name = malloc(strlen(name) + 1)
			};
			
			strcpy(page.id, hash);
			strcpy(page.name, name);
			
			module.pages.items[module.pages.offset++] = page;
		}
		
		resource->modules.items[resource->modules.offset++] = module;
	}
	
	return 0;
}

static int get_page(
	const struct Credentials* const credentials,
	const struct Resource* resource,
	struct Page* page
) {
	
	char authorization[6 + strlen(SPACE) + strlen(credentials->access_token) + 1];
	strcpy(authorization, "Bearer");
	strcat(authorization, SPACE);
	strcat(authorization, credentials->access_token);
	
	const char* const headers[][2] = {
		{"Authorization", authorization},
		{"Club", resource->subdomain},
		{"Referer", "https://hotmart.com"}
	};
	
	struct curl_slist* list = NULL;
	
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
			curl_slist_free_all(list);
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	struct String string = {0};
	
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	
	const char* const endpoint = "https://api-club.hotmart.com/hot-club-api/rest/v3/page";
	
	char url[strlen(endpoint) + strlen(SLASH) + strlen(page->id) + 1];
	strcpy(url, endpoint);
	strcat(url, SLASH);
	strcat(url, page->id);
	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	
	if (curl_easy_perform(curl) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_t* tree = json_loads(string.s, 0, NULL);
	
	string_free(&string);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "mediasSrc");
	
	if (obj != NULL) {
		if (!json_is_array(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t index = 0;
		json_t *item = NULL;
		const size_t array_size = json_array_size(obj);
		
		page->medias.size = sizeof(struct Media) * array_size;
		page->medias.items = malloc(page->medias.size);
		
		json_array_foreach(obj, index, item) {
			if (!json_is_object(item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(item, "mediaSrcUrl");
			
			if (obj == NULL) {
				json_decref(tree);
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				json_decref(tree);
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const media_page = json_string_value(obj);
			
			curl_easy_setopt(curl, CURLOPT_URL, media_page);
			
			if (curl_easy_perform(curl) != CURLE_OK) {
				return UERR_CURL_FAILURE;
			}
			
			const char* const ptr = strstr(string.s, "mediaAssets");
			
			if (ptr == NULL) {
				string_free(&string);
				return UERR_STRSTR_FAILURE;
			}
			
			const char* const start = strstr(ptr, HTTPS_SCHEME);
			const char* const end = strstr(start, QUOTATION_MARK);
			
			size_t size = end - start;
			
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
			
			string_free(&string);
			
			struct Media media = {
				.url = malloc(strlen(url) + 1)
			};
			
			strcpy(media.url, url);
			
			page->medias.items[page->medias.offset++] = media;
		}
		
	}
	
	obj = json_object_get(tree, "attachments");
	
	if (obj != NULL) {
		if (!json_is_array(obj)) {
			json_decref(tree);
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		size_t index = 0;
		json_t *item = NULL;
		const size_t array_size = json_array_size(obj);
		
		page->attachments.size = sizeof(struct Attachment) * array_size;
		page->attachments.items = malloc(page->attachments.size);
		
		json_array_foreach(obj, index, item) {
			if (!json_is_object(item)) {
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const json_t* obj = json_object_get(item, "fileMembershipId");
			
			if (obj == NULL) {
				json_decref(tree);
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				json_decref(tree);
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const id = json_string_value(obj);
			
			const char* const endpoint = "https://api-club.hotmart.com/hot-club-api/rest/v3/attachment";
			
			char url[strlen(endpoint) + strlen(SLASH) + strlen(id) + strlen(SLASH) + 8 + 1];
			strcpy(url, endpoint);
			strcat(url, SLASH);
			strcat(url, id);
			strcat(url, SLASH);
			strcat(url, "download");
			
			curl_easy_setopt(curl, CURLOPT_URL, url);
			
			if (curl_easy_perform(curl) != CURLE_OK) {
				return UERR_CURL_FAILURE;
			}
			
			json_t* tree = json_loads(string.s, 0, NULL);
			
			string_free(&string);
			
			if (tree == NULL) {
				return UERR_JSON_CANNOT_PARSE;
			}
			
			obj = json_object_get(tree, "directDownloadUrl");
			
			if (obj == NULL) {
				json_decref(tree);
				return UERR_JSON_MISSING_REQUIRED_KEY;
			}
			
			if (!json_is_string(obj)) {
				json_decref(tree);
				return UERR_JSON_NON_MATCHING_TYPE;
			}
			
			const char* const download_url = json_string_value(obj);
			
			struct Attachment attachment = {
				.url = malloc(strlen(download_url) + 1)
			};
			
			strcpy(attachment.url, download_url);
			
			page->attachments.items[page->attachments.offset++] = attachment;
			
			json_decref(tree);
		}
	}
	
	json_decref(tree);
	
	return 0;
}

int b() {
	
	if (!directory_exists(APP_CONFIG_DIRECTORY)) {
		fprintf(stderr, "- Diretório de configurações não encontrado, criando-o\r\n");
		
		if (!create_directory(APP_CONFIG_DIRECTORY)) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			exit(EXIT_FAILURE);
		}
		/*
		char* fullpath = NULL;
		
		if (!expand_filename(APP_CONFIG_DIRECTORY, &fullpath)) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			exit(EXIT_FAILURE);
		}
		*/
		
		printf("+ Diretório de configurações criado com sucesso!\r\n");
		
	}
	
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
	
	
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	
	if (curl == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_TCP_FASTOPEN, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36");
	curl_easy_setopt(curl, CURLOPT_DOH_URL, "https://1.0.0.1/dns-query");
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	
	struct curl_slist* resolve_list = NULL;
	
	for (size_t index = 0; index < sizeof(HOSTNAMES) / sizeof(*HOSTNAMES); index++) {
		const char* const hostname = HOSTNAMES[index];
		
		struct curl_slist* tmp = curl_slist_append(resolve_list, hostname);
		
		if (tmp == NULL) {
			curl_slist_free_all(resolve_list);
			return UERR_CURL_FAILURE;
		}
		
		resolve_list = tmp;
	}
	
	curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
	
	int code = 0;
	
	struct Credentials credentials = {0};
	code = authorize(username, password, &credentials);
	
	if (code != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível realizar a autenticação!\r\n");
		exit(EXIT_FAILURE);
	}
	
	printf("+ Usuário autenticado com sucesso!\r\n");
	
	char sha256[(br_sha256_SIZE * 2) + 1];
	sha256_digest(username, sha256);
	
	char filename[strlen(APP_CONFIG_DIRECTORY) + strlen(SLASH) + strlen(sha256) + strlen(DOT) + strlen(JSON_FILE_EXTENSION) + 1];
	strcpy(filename, APP_CONFIG_DIRECTORY);
	strcat(filename, SLASH);
	strcat(filename, sha256);
	strcat(filename, DOT);
	strcat(filename, JSON_FILE_EXTENSION);
	
	printf("+ Exportando arquivo de credenciais\r\n");
	
	FILE* file = fopen(filename, "wb");
	
	if (file == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		exit(EXIT_FAILURE);
	}
	
	json_t* tree = json_object();
	json_object_set_new(tree, "username", json_string(username));
	json_object_set_new(tree, "access_token", json_string(credentials.access_token));
	json_object_set_new(tree, "refresh_token", json_string(credentials.refresh_token));
	json_object_set_new(tree, "expires_in", json_string(credentials.refresh_token));
	
	char* buffer = json_dumps(tree, JSON_COMPACT);
	const size_t buffer_size = strlen(buffer);
	
	const size_t wsize = fwrite(buffer, sizeof(*buffer), buffer_size, file);
	
	free(buffer);
	json_decref(tree);
	fclose(file);
	
	if (wsize != buffer_size) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		exit(EXIT_FAILURE);
	}
	
	printf("+ Arquivo de credenciais exportado com sucesso!\r\n");
	
	printf("+ Obtendo lista de produtos\r\n");
	
	struct Resources resources = {0};
	code = get_resources(&credentials, &resources);
	
	if (code != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível obter a lista de produtos!\r\n");
		exit(EXIT_FAILURE);
	}
	
	printf("+ Selecione o que deseja baixar:\r\n\r\n");
	
	printf("0.\r\nTodos os produtos disponíveis\r\n\r\n");
	
	for (size_t index = 0; index < resources.offset; index++) {
		const struct Resource* resource = &resources.items[index];
		
		printf("%i. \r\nNome: %s\r\nHomepage: https://%s%s\r\n\r\n", index + 1, resource->name, resource->subdomain, HOTMART_CLUB_SUFFIX);
	}
	
	char answer[4 + 1];
	int value = 0;
	
	while (1) {
		printf("> Digite sua escolha: ");
		
		if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
			*strchr(answer, '\n') = '\0';
			
			if (isnumeric(answer)) {
				value = atoi(answer);
				
				if (value >= 0 && value <= resources.offset) {
					break;
				}
			}
		}
		
		fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
	}
	
	struct Resource* download_queue[resources.offset];
	
	size_t queue_count = 0;
	
	if (value == 0) {
		for (size_t index = 0; index < sizeof(download_queue) / sizeof(*download_queue); index++) {
			struct Resource* resource = &resources.items[index];
			download_queue[index] = resource;
			
			queue_count++;
		}
	} else {
		struct Resource* resource = &resources.items[value - 1];
		*download_queue = resource;
		
		queue_count++;
	}
	
	char* cwd = NULL;
	
	if (!expand_filename(".", &cwd)) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		exit(EXIT_FAILURE);
	}
	
	for (size_t index = 0; index < queue_count; index++) {
		struct Resource* resource = &resources.items[index];
		
		printf("+ Obtendo informações sobre o produto\r\n");
		
		get_modules(&credentials, resource);
		
		printf("+ Verificando estado do produto '%s'\r\n", resource->name);
		/*
		char directory[strlen(resource->name) + 1];
		strcpy(directory, resource->name);
		
		normalize_filename(directory);
		
		resource->download_location = malloc(strlen(cwd) + strlen(PATH_SEPARATOR) + strlen(directory) + 1);
		strcpy(resource->download_location, cwd);
		strcat(resource->download_location, PATH_SEPARATOR);
		strcat(resource->download_location, directory);
		
		if (!directory_exists(resource->download_location)) {
			fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", resource->download_location);
			
			if (!create_directory(resource->download_location)) {
				fprintf(stderr, "- Ocorreu um erro ao tentar criar o diretório!\r\n");
				exit(EXIT_FAILURE);
			}
		}
		*/
		for (size_t index = 0; index < resource->modules.offset; index++) {
			struct Module* module = &resource->modules.items[index];
			
			printf("+ Verificando estado do módulo '%s'\r\n", module->name);
			
			printf("Module ID: %s\n", module->id);
			printf("Module name: %s\n", module->name);
			/*
			char directory[strlen(module->name) + 1];
			strcpy(directory, module->name);
			
			normalize_filename(directory);
			
			module->download_location = malloc(strlen(resource->download_location) + strlen(PATH_SEPARATOR) + strlen(directory) + 1);
			strcpy(module->download_location, resource->download_location);
			strcat(module->download_location, PATH_SEPARATOR);
			strcat(module->download_location, directory);
			
			if (!directory_exists(module->download_location)) {
				fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", resource->download_location);
				
				if (!create_directory(module->download_location)) {
					fprintf(stderr, "- Ocorreu um erro ao tentar criar o diretório!\r\n");
					exit(EXIT_FAILURE);
				}
			}
			*/
			for (size_t index = 0; index < module->pages.offset; index++) {
				struct Page* page = &module->pages.items[index];
				
				printf("Page ID: %s\n", page->id);
				printf("Page name: %s\n", page->name);
				
				if (get_page(&credentials, resource, page) == -2) return 1;
				
				for (size_t index = 0; index < page->medias.offset; index++) {
					struct Media* media = &page->medias.items[index];
					
					if (media->url != NULL) {
						printf("Media URL: %s\n", media->url);
					}
				}
				
				for (size_t index = 0; index < page->attachments.offset; index++) {
					struct Attachment* attachment = &page->attachments.items[index];
					
					if (attachment->url != NULL) {
						printf("Attachment URL: %s\n", attachment->url);
					}
				}
			}
			
		}
	}
	
	return 0;
}

int main() {
	printf("%i\n", b());
}