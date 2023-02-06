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
#include "callbacks.h"
#include "types.h"
#include "stringu.h"
#include "query.h"
#include "symbols.h"
#include "curl.h"
#include "iaexpert.h"
#include "html.h"
#include "vimeo.h"

#define IAEXPERT_HOMEPAGE_ENDPOINT "https://iaexpert.academy"

static const char IAEXPERT_AJAX_ENDPOINT[] = 
	IAEXPERT_HOMEPAGE_ENDPOINT
	"/wp-admin/admin-ajax.php";

static const char IAEXPERT_PROFILE_ENDPOINT[] = 
	IAEXPERT_HOMEPAGE_ENDPOINT
	"/alterar_perfil/";

static const char IAEXPERT_COURSES_ENDPOINT[] = 
	IAEXPERT_HOMEPAGE_ENDPOINT
	"/cursos-online-assinatura/";

static const char VIMEO_URL_PATTERN[] = "https://player.vimeo.com/video";

static const tidy_attr_t* attribute_find(
	const tidy_node_t* const root,
	const char* const tag,
	const char* const attribute,
	const char* const attribute_value
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, tag) == 0) {
		const tidy_attr_t* attr = tidy_attr_first(root);
		
		for (const tidy_attr_t* atr = attr; atr != NULL; atr = tidy_attr_next(atr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, attribute) == 0 && strcmp(value, attribute_value) == 0) {
				return attr;
			}
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const tidy_attr_t* attr = attribute_find(next, tag, attribute, attribute_value);
		
		if (attr != NULL) {
			return attr;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const tidy_attr_t* attr = attribute_find(children, tag, attribute, attribute_value);
		
		if (attr != NULL) {
			return attr;
		}
	}
	
	return NULL;
	
}

static int tidy_extract_resources(
	struct Resources* const resources,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "h3") == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "class") == 0 && strcmp(value, "entry-title") == 0) {
				const tidy_node_t* const children = tidy_get_child(root);
				
				if (children == NULL) {
					return UERR_TIDY_ELEMENT_NOT_FOUND;
				}
				
				tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
				tidy_buffer_init(&buffer);
				tidy_node_get_text(document, children, &buffer);
				
				const char* const resource_name = (char*) buffer.bp;
				
				const tidy_node_t* next = root;
				
				for (size_t index = 0; index < 2; index++) {
					next = tidy_get_next(next);
					
					if (next == NULL) {
						return UERR_TIDY_ELEMENT_NOT_FOUND;
					}
				}
				
				const tidy_node_t* const subchildren = tidy_get_child(next);
				
				if (subchildren == NULL) {
					return UERR_TIDY_ELEMENT_NOT_FOUND;
				}
				
				const char* resource_url = NULL;
				
				for (const tidy_attr_t* attr = tidy_attr_first(subchildren); attr != NULL; attr = tidy_attr_next(attr)) {
					const char* const name = tidy_attr_name(attr);
					const char* const value = tidy_attr_value(attr);
					
					if (strcmp(name, "href") != 0) {
						continue;
					}
					
					resource_url = value;
				}
				
				if (resource_url == NULL) {
					return UERR_TIDY_ELEMENT_NOT_FOUND;
				}
				
				const int value = hashs(resource_url);
				
				char resource_id[intlen(value) + 1];
				snprintf(resource_id, sizeof(resource_id), "%i", value);
				
				char* ptr = strchr(resource_name, '\n');
				
				if (ptr != NULL) {
					*ptr = '\0';
				}
				
				struct Resource resource = {
					.id = malloc(strlen(resource_id) + 1),
					.name = malloc(strlen(resource_name) + 1),
					.dirname = malloc(strlen(resource_name) + 1),
					.short_dirname = malloc(strlen(resource_id) + 1),
					.qualification = {0},
					.url = malloc(strlen(resource_url) + 1)
				};
				
				if (resource.id == NULL || resource.name == NULL || resource.dirname == NULL || resource.short_dirname == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(resource.id, resource_id);
				strcpy(resource.name, resource_name);
				
				strcpy(resource.dirname, resource_name);
				normalize_directory(resource.dirname);
				
				strcpy(resource.short_dirname, resource_id);
				
				strcpy(resource.url, resource_url);
				
				const size_t size = resources->size + sizeof(*resources->items) * 1;
				struct Resource* const items = realloc(resources->items, size);
				
				if (items == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				resources->size = size;
				resources->items = items;
				
				resources->items[resources->offset++] = resource;
			}
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_resources(resources, document, children);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_resources(resources, document, next);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_pager_nonce(
	const tidy_doc_t* const document,
	const tidy_node_t* const root,
	const char** nonce
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "div") == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			
			if (strcmp(name, "data-pager-nonce") != 0) {
				continue;
			}
			
			const char* value = tidy_attr_value(attr);
			
			if (value == NULL) {
				return UERR_TIDY_ELEMENT_NOT_FOUND;
			}
			
			*nonce = value;
			
			return UERR_SUCCESS;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_pager_nonce(document, children, nonce);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_pager_nonce(document, next, nonce);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_modules(
	struct Resource* const resource,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL) {
		if (strcmp(name, "div") == 0) {
			for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
				const char* const name = tidy_attr_name(attr);
				
				if (strcmp(name, "class") != 0) {
					continue;
				}
				
				const char* const value = tidy_attr_value(attr);
				
				if (strcmp(value, "ld-item-title") == 0) {
					const size_t size = resource->modules.size + sizeof(*resource->modules.items) * 1;
					struct Module* const items = realloc(resource->modules.items, size);
					
					if (items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					 
					resource->modules.size = size;
					resource->modules.items = items;
					
					const tidy_node_t* const children = tidy_get_child(root);
					
					if (children == NULL) {
						return UERR_TIDY_ELEMENT_NOT_FOUND;
					}
					
					tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
					tidy_buffer_init(&buffer);
					tidy_node_get_text(document, children, &buffer);
					
					const char* const module_name = (char*) buffer.bp;
					
					char* const ptr = strchr(module_name, '\n');
					
					if (ptr != NULL) {
						*ptr = '\0';
					}
					
					const int value = hashs(module_name);
					
					char module_id[intlen(value) + 1];
					snprintf(module_id, sizeof(module_id), "%i", value);
					
					struct Module module = {
						.id = malloc(strlen(module_id) + 1),
						.name = malloc(strlen(module_name) + 1),
						.dirname = malloc(strlen(module_name) + 1),
						.short_dirname = malloc(strlen(module_id) + 1),
						.is_locked = 0
					};
					
					strcpy(module.id, module_id);
					strcpy(module.name, module_name);
					
					strcpy(module.dirname, module_name);
					normalize_directory(module.dirname);
					
					strcpy(module.short_dirname, module_id);
					
					resource->modules.items[resource->modules.offset++] = module;
				} else if (strncmp(value, "ld-table-list-item ", 19) == 0) {
					const tidy_node_t* children = tidy_get_child(root);
					
					if (children == NULL) {
						return UERR_TIDY_ELEMENT_NOT_FOUND;
					}
					
					const char* topic_url = NULL;
					
					for (const tidy_attr_t* attr = tidy_attr_first(children); attr != NULL; attr = tidy_attr_next(attr)) {
						const char* const name = tidy_attr_name(attr);
						const char* const value = tidy_attr_value(attr);
						
						if (strcmp(name, "href") != 0) {
							continue;
						}
						
						topic_url = value;
					}
					
					if (topic_url == NULL) {
						return UERR_TIDY_ELEMENT_NOT_FOUND;
					}
					
					const tidy_node_t* const subchildren = tidy_get_child(children);
					
					if (subchildren == NULL) {
						return UERR_TIDY_ELEMENT_NOT_FOUND;
					}
					
					const tidy_node_t* const next = tidy_get_next(subchildren);
					
					if (next == NULL) {
						return UERR_TIDY_ELEMENT_NOT_FOUND;
					}
					
					tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
					tidy_buffer_init(&buffer);
					tidy_node_get_text(document, tidy_get_child(next), &buffer);
					
					const char* const topic_name = (char*) buffer.bp;
					
					char* const ptr = strchr(topic_name, '\n');
					
					if (ptr != NULL) {
						*ptr = '\0';
					}
					
					const int value = hashs(topic_url);
					
					char topic_id[intlen(value) + 1];
					snprintf(topic_id, sizeof(topic_id), "%i", value);
					
					struct Module* const module = &resource->modules.items[resource->modules.offset - 1];
					
					const size_t size = module->pages.size + sizeof(*module->pages.items) * 1;
					struct Page* const items = realloc(module->pages.items, size);
					
					if (items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					module->pages.size = size;
					module->pages.items = items;
					
					struct Page page = {
						.id = malloc(strlen(topic_id) + 1),
						.name = malloc(strlen(topic_name) + 1),
						.dirname = malloc(strlen(topic_name) + 1),
						.short_dirname = malloc(strlen(topic_id) + 1),
						.is_locked = 0,
						.url = malloc(strlen(topic_url) + 1),
					};
					
					if (page.id == NULL || page.name == NULL || page.dirname == NULL || page.short_dirname == NULL || page.url == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					strcpy(page.id, topic_id);
					strcpy(page.name, topic_name);
					
					strcpy(page.dirname, topic_name);
					normalize_directory(page.dirname);
					
					strcpy(page.short_dirname, topic_id);
					
					strcpy(page.url, topic_url);
					
					module->pages.items[module->pages.offset++] = page;
				}
			}
		} else if (strcmp(name, "a") == 0) {
			int is_pagination = 0;
			
			const char* course_id = NULL;
			const char* lesson_id = NULL;
			const char* paged = NULL;
			const char* context = NULL;
			
			for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
				const char* const name = tidy_attr_name(attr);
				const char* const value = tidy_attr_value(attr);
				
				if (strcmp(name, "aria-label") == 0 && strcmp(value, "Next Page") == 0) {
					is_pagination = 1;
				} else if (strcmp(name, "data-course_id") == 0) {
					course_id = value;
				} else if (strcmp(name, "data-lesson_id") == 0) {
					lesson_id = value;
				} else if (strcmp(name, "data-paged") == 0) {
					paged = value;
				} else if (strcmp(name, "data-context") == 0) {
					context = value;
				}
			}
			
			if (is_pagination) {
				CURL* curl_easy = get_global_curl_easy();
				
				const tidy_node_t* const root = tidy_get_root(document);
				
				const char* nonce = NULL;
				int code = tidy_extract_pager_nonce(document, root, &nonce);
				
				if (code != UERR_SUCCESS) {
					return code;
				}
				
				struct Query query __attribute__((__cleanup__(query_free))) = {0};
				
				add_parameter(&query, "action", "ld30_ajax_pager");
				add_parameter(&query, "ld-topic-page", paged);
				add_parameter(&query, "pager_nonce", nonce);
				add_parameter(&query, "context", context);
				add_parameter(&query, "lesson_id", lesson_id);
				add_parameter(&query, "course_id", course_id);
				
				char* query_fields __attribute__((__cleanup__(charpp_free))) = NULL;
				code = query_stringify(query, &query_fields);
				
				if (code != UERR_SUCCESS) {
					return code;
				}
							
				CURLU* cu __attribute__((__cleanup__(curlupp_free))) = curl_url();
				
				if (cu == NULL) {
					return UERR_CURLU_FAILURE;
				}
				
				if (curl_url_set(cu, CURLUPART_URL, IAEXPERT_AJAX_ENDPOINT, 0) != CURLUE_OK) {
					return UERR_CURLU_FAILURE;
				}
				
				if (curl_url_set(cu, CURLUPART_QUERY, query_fields, 0) != CURLUE_OK) {
					return UERR_CURLU_FAILURE;
				}
				
				char* url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
				
				if (curl_url_get(cu, CURLUPART_URL, &url, 0) != CURLUE_OK) {
					return UERR_CURLU_FAILURE;
				}
				
				struct String string __attribute__((__cleanup__(string_free))) = {0};
				
				curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
				curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
				curl_easy_setopt(curl_easy, CURLOPT_URL, url);
				
				if (curl_easy_perform(curl_easy) != CURLE_OK) {
					return UERR_CURL_FAILURE;
				}
				
				curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
				curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
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
				
				obj = json_object_get(obj, "topics");
				
				if (obj == NULL) {
					return UERR_JSON_MISSING_REQUIRED_KEY;
				}
				
				if (!json_is_string(obj)) {
					return UERR_JSON_NON_MATCHING_TYPE;
				}
				
				const char* const topics = json_string_value(obj);
						
				const tidy_doc_t* const subdocument = tidy_create();
				
				if (subdocument == NULL) {
					return UERR_TIDY_FAILURE;
				}
				
				tidy_opt_set_bool(subdocument, TidyShowWarnings, 0);
				tidy_opt_set_bool(subdocument, TidyShowInfo, 0);
				
				tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
				tidy_buffer_init(&buffer);
				tidy_buffer_append(&buffer, (char*) topics, (uint) strlen(topics));
				
				if (tidy_parse_buffer(subdocument, &buffer) < 0) {
					return UERR_TIDY_FAILURE;
				}
				
				const tidy_node_t* const subroot = tidy_get_root(subdocument);
				
				code = tidy_extract_modules(resource, subdocument, subroot);
				
				if (code != UERR_SUCCESS) {
					return code;
				}
			}
		}
		
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_modules(resource, document, children);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_modules(resource, document, next);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_page_content(
	const tidy_doc_t* const document,
	const tidy_node_t* const root,
	struct Page* const page
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "div") == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			
			if (strcmp(name, "aria-labelledby") != 0) {
				continue;
			}
			
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(value, "content") == 0) {
				const tidy_node_t* const children = tidy_get_child(root);
				
				if (children == NULL) {
					return UERR_TIDY_ELEMENT_NOT_FOUND;
				}
				
				tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
				tidy_buffer_init(&buffer);
				tidy_node_get_text(document, root, &buffer);
				
				page->document.id = malloc(strlen(page->id) + 1);
				page->document.filename = malloc(strlen(page->name) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
				page->document.short_filename = malloc(strlen(page->id) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
				page->document.content = malloc(strlen(HTML_HEADER_START) + strlen((char*) buffer.bp) + strlen(HTML_HEADER_END) + 1);
				
				if (page->document.id == NULL || page->document.filename == NULL || page->document.short_filename == NULL || page->document.content == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(page->document.id, page->id);
				
				strcpy(page->document.filename, page->name);
				strcat(page->document.filename, DOT);
				strcat(page->document.filename, HTML_FILE_EXTENSION);
				
				strcpy(page->document.short_filename, page->id);
				strcat(page->document.short_filename, DOT);
				strcat(page->document.short_filename, HTML_FILE_EXTENSION);
				
				normalize_filename(page->document.filename);
				
				strcpy(page->document.content, HTML_HEADER_START);
				strcat(page->document.content, (char*) buffer.bp);
				strcat(page->document.content, HTML_HEADER_END);
				
				return UERR_SUCCESS;
			}
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_page_content(document, children, page);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_page_content(document, next, page);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

int iaexpert_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	char* user __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, username, 0);
	
	if (user == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char* pass __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, password, 0);
	
	if (pass == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	struct Query query __attribute__((__cleanup__(query_free))) = {0};
	
	add_parameter(&query, "action", "arm_shortcode_form_ajax_action");
	add_parameter(&query, "user_login", user);
	add_parameter(&query, "user_pass", pass);
	add_parameter(&query, "arm_action", "please-login");
	
	char* post_fields __attribute__((__cleanup__(charpp_free))) = NULL;
	const int code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, "");
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	curl_easy_setopt(curl_easy, CURLOPT_URL, IAEXPERT_AJAX_ENDPOINT);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	json_auto_t* tree = json_loads(string.s, 0, NULL);
	
	if (tree == NULL) {
		return UERR_JSON_CANNOT_PARSE;
	}
	
	const json_t* obj = json_object_get(tree, "status");
	
	if (obj == NULL) {
		return UERR_JSON_MISSING_REQUIRED_KEY;
	}
	
	if (!json_is_string(obj)) {
		return UERR_JSON_NON_MATCHING_TYPE;
	}
	
	const char* const status = json_string_value(obj);
	
	if (strcmp(status, "success") != 0) {
		return UERR_PROVIDER_LOGIN_FAILURE;
	}
	
	string_free(&string);
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_URL, IAEXPERT_PROFILE_ENDPOINT);
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, 1L);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, -1);
	
	const tidy_doc_t* const document = tidy_create();
	
	if (document == NULL) {
		return UERR_TIDY_FAILURE;
	}
	
	tidy_opt_set_bool(document, TidyShowWarnings, 0);
	tidy_opt_set_bool(document, TidyShowInfo, 0);
	
	tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
	tidy_buffer_init(&buffer);
	tidy_buffer_append(&buffer, (char*) string.s, (unsigned int) string.slength);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	const tidy_node_t* const root = tidy_get_root(document);
	
	const char* const keys[] = {
		"first_name",
		"last_name"
	};
	
	const char* first_name = NULL;
	const char* last_name = NULL;
	
	for (size_t index = 0; index < sizeof(keys) / sizeof(*keys); index++) {
		const char* const key = keys[index];
		const tidy_attr_t* const attribute = attribute_find(root, "input", "name", key);
		
		for (const tidy_attr_t* attr = attribute; attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "value") != 0) {
				continue;
			}
			
			if (key == keys[0]) {
				first_name = value;
			} else if (key == keys[1]) {
				last_name = value;
			}
		}
	}
	
	credentials->username = malloc(strlen(first_name) + strlen(SPACE) + strlen(last_name) + 1);
	
	if (credentials->username == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->username, first_name);
	strcat(credentials->username, SPACE);
	strcat(credentials->username, last_name);
	
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
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return UERR_SUCCESS;
	
}

int iaexpert_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	(void) credentials;
	
	CURL* curl_easy = get_global_curl_easy();
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_URL, IAEXPERT_COURSES_ENDPOINT);
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, 1L);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, -1);
	
	const char* const pattern = "data-nonce=\"";
	
	const char* start = strstr(string.s, pattern);
	
	if (start == NULL) {
		return UERR_STRSTR_FAILURE;
	}
	
	start += strlen(pattern);
	
	const char* end = strstr(start, QUOTATION_MARK);
	
	const size_t size = (size_t) (end - start);
	
	char nonce[size + 1];
	memcpy(nonce, start, size);
	nonce[size] = '\0';
	
	curl_easy_setopt(curl_easy, CURLOPT_URL, IAEXPERT_AJAX_ENDPOINT);
	
	for (size_t page_number = 1; 1; page_number++) {
		char value[intlen(page_number) + 1];
		snprintf(value, sizeof(value), "%zu", page_number);
		
		struct Query query __attribute__((__cleanup__(query_free))) = {0};
		
		add_parameter(&query, "action", "ld_course_list_shortcode_pager");
		add_parameter(&query, "nonce", nonce);
		add_parameter(&query, "paged", value);
		
		char* post_fields __attribute__((__cleanup__(charpp_free))) = NULL;
		int code = query_stringify(query, &post_fields);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		struct String string __attribute__((__cleanup__(string_free))) = {0};
		
		curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
		curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
		
		if (curl_easy_perform(curl_easy) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
		
		json_auto_t* tree = json_loads(string.s, 0, NULL);
		
		if (tree == NULL) {
			return UERR_JSON_CANNOT_PARSE;
		}
		
		const json_t* const obj = json_object_get(tree, "content");
		
		if (obj == NULL) {
			return UERR_JSON_MISSING_REQUIRED_KEY;
		}
		
		if (!json_is_string(obj)) {
			return UERR_JSON_NON_MATCHING_TYPE;
		}
		
		const char* const content = json_string_value(obj);
		
		const tidy_doc_t* const document = tidy_create();
		
		if (document == NULL) {
			return UERR_TIDY_FAILURE;
		}
		
		tidy_opt_set_bool(document, TidyShowWarnings, 0);
		tidy_opt_set_bool(document, TidyShowInfo, 0);
		
		tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
		tidy_buffer_init(&buffer);
		tidy_buffer_append(&buffer, (char*) content, (uint) strlen(content));
		
		if (tidy_parse_buffer(document, &buffer) < 0) {
			return UERR_TIDY_FAILURE;
		}
		
		const tidy_node_t* const root = tidy_get_root(document);
		
		const size_t oldoffset = resources->offset;
		
		code = tidy_extract_resources(resources, document, root);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		if (oldoffset == resources->offset) {
			break;
		}
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_COOKIELIST, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	return UERR_SUCCESS;
	
}

int iaexpert_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_URL, resource->url);
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, 1L);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, -1);
	
	const char* const pattern = "> Não Matriculado<";
	
	if (strstr(string.s, pattern) != NULL) {
		const char* pattern = "/sfwd-courses/";
		
		const char* start = strstr(string.s, pattern);
		
		if (start == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		start += strlen(pattern);
		
		const char* end = strstr(start, QUOTATION_MARK);
		
		if (end == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		size_t size = (size_t) (end - start);
		
		char course_id[size + 1];
		memcpy(course_id, start, size);
		course_id[size] = '\0';
		
		pattern = "name=\"course_join\" value=\"";
	
		start = strstr(string.s, pattern);
		
		if (start == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		start += strlen(pattern);
		
		end = strstr(start, QUOTATION_MARK);
		
		if (end == NULL) {
			return UERR_STRSTR_FAILURE;
		}
		
		size = (size_t) (end - start);
		
		char course_join[size + 1];
		memcpy(course_join, start, size);
		course_join[size] = '\0';
		
		struct Query query __attribute__((__cleanup__(query_free))) = {0};
		
		add_parameter(&query, "course_id", course_id);
		add_parameter(&query, "course_join", course_join);
		
		char* post_fields __attribute__((__cleanup__(charpp_free))) = NULL;
		const int code = query_stringify(query, &post_fields);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
		
		string_free(&string);
		
		if (curl_easy_perform(curl_easy) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
		
		curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	}
	
	const tidy_doc_t* const document = tidy_create();
	
	if (document == NULL) {
		return UERR_TIDY_FAILURE;
	}
	
	tidy_opt_set_bool(document, TidyShowWarnings, 0);
	tidy_opt_set_bool(document, TidyShowInfo, 0);
	
	tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
	tidy_buffer_init(&buffer);
	tidy_buffer_append(&buffer, string.s, (uint) string.slength);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	const tidy_node_t* const root = tidy_get_root(document);
	
	const int code = tidy_extract_modules(resource, document, root);
	
	curl_easy_setopt(curl_easy, CURLOPT_COOKIELIST, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return code;
	
}

int iaexpert_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	(void) credentials;
	(void) resource;
	(void) module;
	
	return UERR_NOT_IMPLEMENTED;
	
}

int iaexpert_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_URL, page->url);
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, 1L);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(curl_easy, CURLOPT_MAXREDIRS, -1);
	
	const tidy_doc_t* const document = tidy_create();
	
	if (document == NULL) {
		return UERR_TIDY_FAILURE;
	}
	
	tidy_opt_set_bool(document, TidyShowWarnings, 0);
	tidy_opt_set_bool(document, TidyShowInfo, 0);
	
	tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
	tidy_buffer_init(&buffer);
	tidy_buffer_append(&buffer, string.s, (uint) string.slength);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	const tidy_node_t* const root = tidy_get_root(document);
	
	int code = tidy_extract_page_content(document, root, page);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	string_array_t items __attribute__((__cleanup__(string_array_free))) = {0};
	code = attribute_find_all(&items, root, "iframe", "src");
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	for (size_t index = 0; index < items.offset; index++) {
		const char* const url = items.items[index];
		
		printf("+ A mídia localizada em '%s' aponta para uma fonte externa, verificando se é possível processá-la\r\n", url);
		
		struct Media media = {0};
		
		if (memcmp(url, VIMEO_URL_PATTERN, strlen(VIMEO_URL_PATTERN)) == 0) {
			const int code = vimeo_parse(url, resource, page, &media, page->url);
			
			if (!(code == UERR_SUCCESS || code == UERR_NO_STREAMS_AVAILABLE)) {
				return code;
			}
		}
		
		if (media.video.url == NULL) {
			 fprintf(stderr, "- A URL é inválida ou não foi reconhecida. Por favor, reporte-a ao desenvolvedor.\r\n");
		} else {
			const size_t size = page->medias.size + sizeof(struct Media) * 1;
			struct Media* const items = (struct Media*) realloc(page->medias.items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			page->medias.size = size;
			page->medias.items = items;
			
			page->medias.items[page->medias.offset++] = media;
		}
	}
	
	return UERR_SUCCESS;
	
}
