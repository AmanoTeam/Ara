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
#include "qconcursos.h"
#include "html.h"
#include "m3u8.h"

#define QCONCURSOS_HOMEPAGE_ENDPOINT "https://www.qconcursos.com"
#define QCONCURSOS_APP_HOMEPAGE_ENDPOINT "https://app.qconcursos.com"

static const char QCONCURSOS_LOGIN_ENDPOINT[] = 
	QCONCURSOS_HOMEPAGE_ENDPOINT
	"/conta/entrar";

static const char QCONCURSOS_PROFILE_ENDPOINT[] = 
	QCONCURSOS_HOMEPAGE_ENDPOINT
	"/usuario/configuracoes/dados-da-conta";

static const char QCONCURSOS_COURSES_ENDPOINT[] = 
	QCONCURSOS_APP_HOMEPAGE_ENDPOINT
	"/components/tracks/index/list";

static const char QCONCURSOS_COURSE_ENDPOINT[] = 
	QCONCURSOS_APP_HOMEPAGE_ENDPOINT
	"/cursos";

static const char QCONCURSOS_MODULE_ENDPOINT[] = 
	QCONCURSOS_APP_HOMEPAGE_ENDPOINT
	"/components/tracks/show/topics";

static const char QCONCURSOS_STREAMS_ENDPOINT[] = 
	QCONCURSOS_APP_HOMEPAGE_ENDPOINT
	"/components/chapters/show/toolbar/sidebars/lessons_list";

static const char HTTP_HEADER_CONTENT_TYPE[] = "Content-Type";
static const char MIME_TYPE_JSON[] = "application/json";

static const char CAPITULOS[] = "capitulos";

static int tidy_extract_authenticity_token(
	const tidy_doc_t* const document,
	const tidy_node_t* const root,
	const char** authenticity_token
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "input") == 0) {
		const char* token = NULL;
		int matches = 0;
		
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "name") == 0 && strcmp(value, "authenticity_token") == 0) {
				matches = 1;
			} else if (strcmp(name, "value") == 0) {
				token = (const char*) value;
			}
		}
		
		if (matches) {
			if (token == NULL) {
				return UERR_TIDY_ELEMENT_NOT_FOUND;
			}
			
			*authenticity_token = token;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_authenticity_token(document, children, authenticity_token);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_authenticity_token(document, next, authenticity_token);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_username(
	const tidy_doc_t* const document,
	const tidy_node_t* const root,
	const char** username
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "div") == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "data-name") == 0) {
				*username = (const char*) value;
				return UERR_SUCCESS;
			}
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_username(document, children, username);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_username(document, next, username);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_resources(
	struct Resources* const resources,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "a") == 0) {
		const char* resource_id = NULL;
		const char* resource_name = NULL;
		
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "data-title") == 0) {
				resource_name = value;
			} else if (strcmp(name, "data-track-id") == 0) {
				resource_id = value;
			}
		}
		
		if (!(resource_id == NULL || resource_name == NULL)) {
			if ((sizeof(*resources->items) * (resources->offset + 1)) > resources->size) {
				return UERR_BUFFER_OVERFLOW_FAILURE;
			}
			
			char resource_url[strlen(QCONCURSOS_COURSE_ENDPOINT) + strlen(SLASH) + strlen(resource_id) + 1];
			strcpy(resource_url, QCONCURSOS_COURSE_ENDPOINT);
			strcat(resource_url, SLASH);
			strcat(resource_url, resource_id);
			
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
			
			resources->items[resources->offset++] = resource;
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

static int tidy_extract_modules(
	struct Resource* const resource,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "label") == 0) {
		const char* module_id = NULL;
		const char* module_name = NULL;
		
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "data-event-section-id") == 0) {
				module_id = value;
			} else if (strcmp(name, "data-event-section-title") == 0) {
				module_name = value;
			}
		}
		
		if (!(module_id == NULL || module_name == NULL)) {
			const size_t size = resource->modules.size + sizeof(*resource->modules.items) * 1;
			struct Module* const items = realloc(resource->modules.items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			resource->modules.size = size;
			resource->modules.items = items;
			
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

static int tidy_extract_pages(
	struct Module* const module,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "a") == 0) {
		const char* page_url = NULL;
		const char* page_name = NULL;
		
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "data-section-name") == 0) {
				page_name = value;
			} else if (strcmp(name, "href") == 0) {
				page_url = value;
			}
		}
		
		if (!(page_url == NULL || page_name == NULL)) {
			const size_t size = module->pages.size + sizeof(*module->pages.items) * 1;
			struct Page* const items = realloc(module->pages.items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			module->pages.size = size;
			module->pages.items = items;
			
			const char* page_id = basename(page_url);
			
			if (!isnumeric(page_id)) {
				return UERR_STRSTR_FAILURE;
			}
			
			struct Page page = {
				.id = malloc(strlen(page_id) + 1),
				.name = malloc(strlen(page_name) + 1),
				.dirname = malloc(strlen(page_name) + 1),
				.short_dirname = malloc(strlen(page_id) + 1),
				.is_locked = 0
			};
			
			strcpy(page.id, page_id);
			strcpy(page.name, page_name);
			
			strcpy(page.dirname, page_name);
			normalize_directory(page.dirname);
			
			strcpy(page.short_dirname, page_id);
			
			module->pages.items[module->pages.offset++] = page;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_pages(module, document, children);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_pages(module, document, next);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_topic_id(
	const tidy_doc_t* const document,
	const tidy_node_t* const root,
	const char** topic_id
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "a") == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "data-topic-id") == 0) {
				*topic_id = (const char*) value;
				return UERR_SUCCESS;
			}
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_topic_id(document, children, topic_id);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_topic_id(document, next, topic_id);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_streams(
	struct Page* const page,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "div") == 0) {
		const char* stream_id = NULL;
		const char* stream_url = NULL;
		
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, "data-url") == 0) {
				stream_url = value;
			} else if (strcmp(name, "data-resource-id") == 0) {
				stream_id = value;
			}
		}
		
		if (!(stream_id == NULL || stream_url == NULL)) {
			CURL* curl_easy = get_global_curl_easy();
			
			buffer_t string __buffer_free__ = {0};
			
			curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
			curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
			curl_easy_setopt(curl_easy, CURLOPT_URL, stream_url);
			
			if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
				return UERR_CURL_FAILURE;
			}
				
			struct Tags tags = {0};
			
			if (m3u8_parse(&tags, string.s) != UERR_SUCCESS) {
				return UERR_M3U8_PARSE_FAILURE;
			}
			
			int last_width = 0;
			
			const char* video_stream = NULL;
			const char* audio_stream = NULL;
			
			for (size_t index = 0; index < tags.offset; index++) {
				const struct Tag* const tag = &tags.items[index];
				
				switch (tag->type) {
					case EXT_X_STREAM_INF: {
						const struct Attribute* const attribute = attributes_get(&tag->attributes, "RESOLUTION");
						
						if (attribute == NULL) {
							return UERR_M3U8_MISSING_REQUIRED_ATTRIBUTE;
						}
						
						const char* const start = attribute->value;
						const char* const end = strstr(start, "x");
						
						const size_t size = (size_t) (end - start);
						
						char value[size + 1];
						memcpy(value, start, size);
						value[size] = '\0';
						
						const int width = atoi(value);
						
						if (last_width < width) {
							last_width = width;
							video_stream = tag->uri;
						}
						
						break;
					}
					case EXT_X_MEDIA: {
						if (audio_stream != NULL) {
							break;
						}
						
						const struct Attribute* attribute = attributes_get(&tag->attributes, "TYPE");
						
						if (attribute == NULL) {
							return UERR_M3U8_MISSING_REQUIRED_ATTRIBUTE;
						}
						
						if (strcmp(attribute->value, "AUDIO") != 0) {
							break;
						}
						
						 attribute = attributes_get(&tag->attributes, "URI");
						
						if (attribute == NULL) {
							return UERR_M3U8_MISSING_REQUIRED_ATTRIBUTE;
						}
						
						audio_stream = attribute->value;
						
						break;
					}
					default: {
						break;
					}
				}
			}
			
			if (video_stream == NULL || audio_stream == NULL) {
				return UERR_NO_STREAMS_AVAILABLE;
			}
			
			struct Media media = {
				.type = MEDIA_M3U8,
				.audio = {
					.id = malloc(strlen(stream_id) + 1),
					.filename = malloc(strlen(stream_id) + strlen(DOT) + strlen(AAC_FILE_EXTENSION) + 1),
					.short_filename = malloc(strlen(stream_id) + strlen(DOT) + strlen(AAC_FILE_EXTENSION) + 1)
				},
				.video = {
					.id = malloc(strlen(stream_id) + 1),
					.filename = malloc(strlen(stream_id) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1),
					.short_filename = malloc(strlen(stream_id) + strlen(DOT) + strlen(MP4_FILE_EXTENSION) + 1)
				}
			};
			
			if (media.audio.id == NULL || media.audio.filename == NULL || media.audio.short_filename == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			if (media.video.id == NULL || media.video.filename == NULL || media.video.short_filename == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(media.audio.id, stream_id);
			
			strcpy(media.audio.filename, stream_id);
			strcat(media.audio.filename, DOT);
			strcat(media.audio.filename, AAC_FILE_EXTENSION);
			
			strcpy(media.audio.short_filename, stream_id);
			strcat(media.audio.short_filename, DOT);
			strcat(media.audio.short_filename, AAC_FILE_EXTENSION);
			
			normalize_filename(media.audio.filename);
			
			strcpy(media.video.id, stream_id);
			
			strcpy(media.video.filename, stream_id);
			strcat(media.video.filename, DOT);
			strcat(media.video.filename, MP4_FILE_EXTENSION);
			
			strcpy(media.video.short_filename, stream_id);
			strcat(media.video.short_filename, DOT);
			strcat(media.video.short_filename, MP4_FILE_EXTENSION);
			
			normalize_filename(media.video.filename);
			
			CURLU* cu __attribute__((__cleanup__(curlupp_free))) = curl_url();
			
			if (cu == NULL) {
				return UERR_CURLU_FAILURE;
			}
			
			if (curl_url_set(cu, CURLUPART_URL, stream_url, 0) != CURLUE_OK) {
				return UERR_CURLU_FAILURE;
			}
			
			if (curl_url_set(cu, CURLUPART_URL, video_stream, 0) != CURLUE_OK) {
				return UERR_CURLU_FAILURE;
			}
			
			if (curl_url_get(cu, CURLUPART_URL, &media.video.url, 0) != CURLUE_OK) {
				return UERR_CURLU_FAILURE;
			}
			
			if (curl_url_set(cu, CURLUPART_URL, stream_url, 0) != CURLUE_OK) {
				return UERR_CURLU_FAILURE;
			}
			
			if (curl_url_set(cu, CURLUPART_URL, audio_stream, 0) != CURLUE_OK) {
				return UERR_CURLU_FAILURE;
			}
			
			if (curl_url_get(cu, CURLUPART_URL, &media.audio.url, 0) != CURLUE_OK) {
				return UERR_CURLU_FAILURE;
			}
			
			const size_t size = page->medias.size + sizeof(*page->medias.items) * 1;
			struct Media* items = (struct Media*) realloc(page->medias.items, size);
			
			if (items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			page->medias.size = size;
			page->medias.items = items;
			
			page->medias.items[page->medias.offset++] = media;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_streams(page, document, children);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_streams(page, document, next);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

static int tidy_extract_page_content(
	struct Page* const page,
	const tidy_doc_t* const document,
	const tidy_node_t* const root
) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, "div") == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (!(strcmp(name, "data-target") == 0 && strcmp(value, "content-wrapper.content") == 0)) {
				continue;
			}
			
			tidy_buffer_t buffer __attribute__((__cleanup__(tidy_buffer_free))) = {0};
			tidy_buffer_init(&buffer);
			tidy_node_get_text(document, root, &buffer);
			
			page->document.id = malloc(strlen(page->id) + 1);
			page->document.filename = malloc(strlen(page->name) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
			page->document.short_filename = malloc(strlen(page->id) + strlen(DOT) + strlen(HTML_FILE_EXTENSION) + 1);
			page->document.content = malloc(strlen(HTML_HEADER_START) + strlen((const char*) buffer.bp) + strlen(HTML_HEADER_END) + 1);
			
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
			strcat(page->document.content, (const char*) buffer.bp);
			strcat(page->document.content, HTML_HEADER_END);
			
			return UERR_SUCCESS;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = tidy_extract_page_content(page, document, children);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = tidy_extract_page_content(page, document, next);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}

int qconcursos_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, "");
	curl_easy_setopt(curl_easy, CURLOPT_URL, QCONCURSOS_LOGIN_ENDPOINT);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
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
	
	buffer_free(&string);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	const tidy_node_t* root = tidy_get_root(document);
	
	const char* authenticity_token = NULL;
	int code = tidy_extract_authenticity_token(document, root, &authenticity_token);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	char* user __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, username, 0);
	
	if (user == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char* pass __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, password, 0);
	
	if (pass == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	char* token __attribute__((__cleanup__(curlcharpp_free))) = curl_easy_escape(NULL, authenticity_token, 0);
	
	if (token == NULL) {
		return UERR_CURL_FAILURE;
	}
	
	struct Query query __query_free__ = {0};
	
	add_parameter(&query, "authenticity_token", token);
	add_parameter(&query, "user[email]", user);
	add_parameter(&query, "user[password]", pass);
	add_parameter(&query, "commit", "Entrar");
	
	char* post_fields __free__ = NULL;
	code = query_stringify(query, &post_fields);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_discard_body_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	
	long status_code = 0;
	char* location = NULL;
	
	if (curl_easy_getinfo(curl_easy, CURLINFO_RESPONSE_CODE, &status_code) != CURLE_OK) {
		return UERR_CURL_GETINFO_FAILURE;
	}
	
	if (curl_easy_getinfo(curl_easy, CURLINFO_REDIRECT_URL, &location) != CURLE_OK) {
		return UERR_CURL_GETINFO_FAILURE;
	}
	
	if (!(status_code == 302 && strcmp(location, "https://app.qconcursos.com/") == 0)) {
		return UERR_PROVIDER_LOGIN_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl_easy, CURLOPT_URL, QCONCURSOS_PROFILE_ENDPOINT);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	tidy_buffer_free(&buffer);
	tidy_buffer_append(&buffer, string.s, (uint) string.slength);
	
	buffer_free(&string);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	root = tidy_get_root(document);
	
	const char* account_name = NULL;
	code = tidy_extract_username(document, root, &account_name);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (account_name == NULL) {
		return UERR_TIDY_ELEMENT_NOT_FOUND;
	}
	
	credentials->username = malloc(strlen(account_name) + 1);
	
	if (credentials->username == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(credentials->username, account_name);
	
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

int qconcursos_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	(void) credentials;
	
	CURL* curl_easy = get_global_curl_easy();
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_URL, QCONCURSOS_COURSES_ENDPOINT);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	
	const char* const headers[][2] = {
		{HTTP_HEADER_CONTENT_TYPE, MIME_TYPE_JSON}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
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
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	
	for (size_t page_number = 1; 1; page_number++) {
		json_auto_t* tree = json_object();
		json_object_set_new(tree, "type", json_string("page_component"));
		
		json_t* data = json_object();
		json_object_set_new(data, "page", json_integer(page_number));
		json_object_set_new(data, "per_page", json_integer(100));
		json_object_set_new(tree, "data", data);
		
		char* post_fields __free__ = json_dumps(tree, JSON_COMPACT);
		
		if (post_fields == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		buffer_t string __buffer_free__ = {0};
		
		curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
		curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
		
		if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
			return UERR_CURL_FAILURE;
		}
		
		if (page_number == 1) {
			const char* const pattern = "Encontramos <strong>";
			const char* start = strstr(string.s, pattern);
			
			if (start == NULL) {
				return UERR_STRSTR_FAILURE;
			}
			
			start += strlen(pattern);
			
			const char* end = strstr(start, LESS_THAN);
			
			if (end == NULL) {
				return UERR_STRSTR_FAILURE;
			}
			
			const size_t size = (size_t) (end - start);
			
			char s[size + 1];
			memcpy(s, start, size);
			s[size] = '\0';
			
			const size_t total_courses = (size_t) atoi(s);
			
			resources->size = sizeof(*resources->items) * total_courses;
			resources->items = malloc(resources->size);
			
			if (resources->items == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
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
		
		const size_t oldoffset = resources->offset;
		
		const int code = tidy_extract_resources(resources, document, root);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
		
		if (resources->offset < 1) {
			return UERR_TIDY_ELEMENT_NOT_FOUND;
		}
		
		if (resources->offset == oldoffset) {
			break;
		}
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	return UERR_SUCCESS;
	
}

int qconcursos_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_URL, resource->url);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
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
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (resource->modules.offset < 1) {
		return UERR_TIDY_ELEMENT_NOT_FOUND;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_COOKIELIST, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	
	return code;
	
}

int qconcursos_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	curl_easy_setopt(curl_easy, CURLOPT_URL, QCONCURSOS_MODULE_ENDPOINT);
	
	const char* const headers[][2] = {
		{HTTP_HEADER_CONTENT_TYPE, MIME_TYPE_JSON}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
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
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	
	json_auto_t* tree = json_object();
	json_object_set_new(tree, "type", json_string("page_component"));
		
	json_t* data = json_object();
	json_object_set_new(data, "section_id", json_string(module->id));
	json_object_set_new(data, "track_id", json_string(resource->id));
	json_object_set_new(tree, "data", data);
	
	char* post_fields __free__ = json_dumps(tree, JSON_COMPACT);
	
	if (post_fields == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
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
	
	const int code = tidy_extract_pages(module, document, root);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (module->pages.offset < 1) {
		return UERR_TIDY_ELEMENT_NOT_FOUND;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	return code;
	
}

int qconcursos_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	CURL* curl_easy = get_global_curl_easy();
	
	buffer_t string __buffer_free__ = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, credentials->cookie_jar);
	
	char url[strlen(QCONCURSOS_COURSE_ENDPOINT) + strlen(SLASH) + strlen(resource->id) + strlen(SLASH) + strlen(CAPITULOS) + strlen(SLASH) + strlen(page->id) + 1];
	strcpy(url, QCONCURSOS_COURSE_ENDPOINT);
	strcat(url, SLASH);
	strcat(url, resource->id);
	strcat(url, SLASH);
	strcat(url, CAPITULOS);
	strcat(url, SLASH);
	strcat(url, page->id);
	
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
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
	
	buffer_free(&string);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	const tidy_node_t* root = tidy_get_root(document);
	
	int code = tidy_extract_page_content(page, document, root);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (page->document.id == NULL) {
		return UERR_TIDY_ELEMENT_NOT_FOUND;
	}
	
	const char* topic_id = NULL;
	code = tidy_extract_topic_id(document, root, &topic_id);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	if (topic_id == NULL) {
		return UERR_TIDY_ELEMENT_NOT_FOUND;
	}
	
	const char* const headers[][2] = {
		{HTTP_HEADER_CONTENT_TYPE, MIME_TYPE_JSON}
	};
	
	struct curl_slist* list __attribute__((__cleanup__(curl_slistp_free_all))) = NULL;
	
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
			return UERR_CURL_FAILURE;
		}
		
		list = tmp;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, list);
	
	json_auto_t* tree = json_object();
	json_object_set_new(tree, "type", json_string("page_component"));
		
	json_t* data = json_object();
	json_object_set_new(data, "chapter_id", json_string(page->id));
	json_object_set_new(data, "topic_id", json_string(topic_id));
	json_object_set_new(tree, "data", data);
	
	char* post_fields __free__ = json_dumps(tree, JSON_COMPACT);
	
	if (post_fields == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, post_fields);
	curl_easy_setopt(curl_easy, CURLOPT_URL, QCONCURSOS_STREAMS_ENDPOINT);
	
	if (curl_easy_perform_retry(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COOKIEFILE, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_COPYPOSTFIELDS, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_HTTPGET, 1L);
	
	tidy_buffer_free(&buffer);
	tidy_buffer_append(&buffer, string.s, (uint) string.slength);
	
	buffer_free(&string);
	
	if (tidy_parse_buffer(document, &buffer) < 0) {
		return UERR_TIDY_FAILURE;
	}
	
	root = tidy_get_root(document);
	
	code = tidy_extract_streams(page, document, root);
	
	if (code != UERR_SUCCESS) {
		return code;
	}
	
	return UERR_SUCCESS;
	
}
