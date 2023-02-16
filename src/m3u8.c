#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "m3u8.h"
#include "errors.h"
#include "symbols.h"
#include "fstream.h"

static const enum Type TYPES[] = {
	EXTM3U,
	EXT_X_VERSION,
	EXTINF,
	EXT_X_BYTERANGE,
	EXT_X_DISCONTINUITY,
	EXT_X_KEY,
	EXT_X_MAP,
	EXT_X_PROGRAM_DATE_TIME,
	EXT_X_DATERANGE,
	EXT_X_TARGETDURATION,
	EXT_X_MEDIA_SEQUENCE,
	EXT_X_DISCONTINUITY_SEQUENCE,
	EXT_X_ENDLIST,
	EXT_X_PLAYLIST_TYPE,
	EXT_X_I_FRAMES_ONLY,
	EXT_X_MEDIA,
	EXT_X_STREAM_INF,
	EXT_X_I_FRAME_STREAM_INF,
	EXT_X_SESSION_DATA,
	EXT_X_SESSION_KEY,
	EXT_X_INDEPENDENT_SEGMENTS,
	EXT_X_START
};

const char* tag_stringify(const enum Type type) {
	
	switch (type) {
		case EXTM3U:
			return "EXTM3U";
		case EXT_X_VERSION:
			return "EXT-X-VERSION";
		case EXTINF:
			return "EXTINF";
		case EXT_X_BYTERANGE:
			return "EXT-X-BYTERANGE";
		case EXT_X_DISCONTINUITY:
			return "EXT-X-DISCONTINUITY";
		case EXT_X_KEY:
			return "EXT-X-KEY";
		case EXT_X_MAP:
			return "EXT-X-MAP";
		case EXT_X_PROGRAM_DATE_TIME:
			return "EXT-X-PROGRAM-DATE-TIME";
		case EXT_X_DATERANGE:
			return "EXT-X-DATERANGE";
		case EXT_X_TARGETDURATION:
			return "EXT-X-TARGETDURATION";
		case EXT_X_MEDIA_SEQUENCE:
			return "EXT-X-MEDIA-SEQUENCE";
		case EXT_X_DISCONTINUITY_SEQUENCE:
			return "EXT-X-DISCONTINUITY-SEQUENCE";
		case EXT_X_ENDLIST:
			return "EXT-X-ENDLIST";
		case EXT_X_PLAYLIST_TYPE:
			return "EXT-X-PLAYLIST-TYPE";
		case EXT_X_I_FRAMES_ONLY:
			return "EXT-X-I-FRAMES-ONLY";
		case EXT_X_MEDIA:
			return "EXT-X-MEDIA";
		case EXT_X_STREAM_INF:
			return "EXT-X-STREAM-INF";
		case EXT_X_I_FRAME_STREAM_INF:
			return "EXT-X-I-FRAME-STREAM-INF";
		case EXT_X_SESSION_DATA:
			return "EXT-X-SESSION-DATA";
		case EXT_X_SESSION_KEY:
			return "EXT-X-SESSION-KEY";
		case EXT_X_INDEPENDENT_SEGMENTS:
			return "EXT-X-INDEPENDENT-SEGMENTS";
		case EXT_X_START:
			return "EXT-X-START";
	}
	
	return NULL;
	
}

static enum Type get_tag(const char* const name) {
	
	for (size_t index = 0; index < sizeof(TYPES) / sizeof(*TYPES); index++) {
		const enum Type tag_type = TYPES[index];
		const char* const tag_name = tag_stringify(tag_type);
		
		if (strcmp(name, tag_name) == 0) {
			return tag_type;
		}
	}
	
	return (enum Type) 0;
	
}

int m3u8_parse(struct Tags* tags, const char* const s) {
	
	const char* text_end = strchr(s, '\0');
	
	const char* line_start = s;
	const char* line_end = strstr(line_start, LF);
	
	while (1) {
		if (line_end == NULL) {
			line_end = text_end;
		}
		
		const size_t line_size = (size_t) (line_end - line_start);
		
		if (line_size > 0) {
			const char* start = line_start;
			const char* end = line_end;
			
			while (start != end) {
				const char ch = *start;
				
				if (!isspace(ch)) {
					break;
				}
				
				start++;
			}
			
			size_t line_size = (size_t) (end - start);
			end--;
			
			while (end != start) {
				if (!isspace(*end)) {
					break;
				}
				
				end--;
				line_size--;
			}
			
			if (line_size > 0) {
				char line[line_size + 1];
				memcpy(line, start, line_size);
				line[line_size] = '\0';
				
				struct Tag tag = {0};
				
				if (*line == *HASHTAG) {
					const char* const start = line + 1;
					const char* separator = strstr(start, COLON);
					
					if (separator == NULL) {
						tag.type = get_tag(start);
					} else {
						const size_t name_size = (size_t) (separator - start);
						
						char name[name_size + 1];
						memcpy(name, start, name_size);
						name[name_size] = '\0';
						
						tag.type = get_tag(name);
						
						const char* const line_end = strchr(line, '\0');
						
						separator++;
						
						const char* attribute_start = separator;
						const char* attribute_end = strstr(attribute_start, COMMA);
						
						while (1) {
							if (attribute_end == NULL) {
								attribute_end = line_end;
							}
							
							const size_t attribute_size = (size_t) (attribute_end - attribute_start);
							
							if (attribute_size > 0) {
								char attribute[attribute_size + 1];
								memcpy(attribute, attribute_start, attribute_size);
								attribute[attribute_size] = '\0';
								
								const char* separator = strstr(attribute_start, EQUAL);
								
								if (separator == NULL) {
									const size_t size = (size_t) (attribute_end - attribute_start);
									
									tag.value = (char*) malloc(size + 1);
									
									if (tag.value == NULL) {
										return UERR_MEMORY_ALLOCATE_FAILURE;
									}
									
									memcpy(tag.value, attribute_start, size);
									tag.value[size] = '\0';
								} else {
									struct Attribute attr = {0};
									
									const size_t key_size = (size_t) (separator - attribute_start);
									
									if (key_size > 0) {
										attr.key = (char*) malloc(key_size + 1);
										
										if (attr.key == NULL) {
											return UERR_MEMORY_ALLOCATE_FAILURE;
										}
										
										memcpy(attr.key, attribute_start, key_size);
										attr.key[key_size] = '\0';
									}
									
									if (separator != attribute_end) {
										separator++;
									}
									
									if (*separator == *QUOTATION_MARK) {
										separator++;
										
										const char* value_end = strstr(separator, QUOTATION_MARK);
										
										if (value_end == NULL) {
											return UERR_M3U8_UNTERMINATED_STRING_LITERAL;
										}
										
										attribute_end = value_end;
										attr.is_quoted = 1;
									}
									
									const size_t value_size = (size_t) (separator == attribute_end ? 0 : attribute_end - separator);
									
									if (value_size > 0) {
										attr.value = (char*) malloc(value_size + 1);
										
										if (attr.value == NULL) {
											return UERR_MEMORY_ALLOCATE_FAILURE;
										}
										
										memcpy(attr.value, separator, value_size);
										attr.value[value_size] = '\0';
									}
									
									const size_t size = tag.attributes.size + sizeof(struct Attribute) * 1;
									struct Attribute* items = (struct Attribute*) realloc(tag.attributes.items, size);
									
									if (items == NULL) {
										return UERR_MEMORY_ALLOCATE_FAILURE;
									}
									
									tag.attributes.items = items;
									tag.attributes.size = size;
									
									tag.attributes.items[tag.attributes.offset++] = attr;
								}
							}
							
							if (attribute_end == line_end) {
								break;
							}
							
							attribute_start = attribute_end;
							attribute_start++;
							
							if (*attribute_start == *COMMA) {
								attribute_start++;
							}
							
							attribute_end = strstr(attribute_start, COMMA);
						}
					}
					
					const size_t size = tags->size + sizeof(struct Tag) * 1;
					struct Tag* items = (struct Tag*) realloc(tags->items, size);
					
					if (items == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					tags->items = items;
					tags->size = size;
					
					tags->items[tags->offset++] = tag;
				} else if (tags->offset > 0) {
					struct Tag* tag = &tags->items[tags->offset - 1];
					tag->uri = malloc(sizeof(line));
					
					if (tag->uri == NULL) {
						return UERR_MEMORY_ALLOCATE_FAILURE;
					}
					
					strcpy(tag->uri, line);
				}
			}
		}
		
		if (line_end == text_end) {
			break;
		}
		
		line_start = line_end;
		line_start++;
		
		line_end = strstr(line_start, LF);
	}
	
	return UERR_SUCCESS;
	
}

void m3u8_free(struct Tags* tags) {

	for (size_t index = 0; index < tags->offset; index++) {
		struct Tag* tag = &tags->items[index];
		
		for (size_t index = 0; index < tag->attributes.offset; index++) {
			struct Attribute* attribute = &tag->attributes.items[index];
			
			free(attribute->key);
			attribute->key = NULL;
			
			free(attribute->value);
			attribute->value = NULL;
		}
		
		tag->attributes.offset = 0;
		tag->attributes.size = 0;
		free(tag->attributes.items);
		tag->attributes.items = NULL;
		
		if (tag->uri != NULL) {
			free(tag->uri);
			tag->uri = NULL;
		}
	}
	
	tags->offset = 0;
	tags->size = 0;
	free(tags->items);
	tags->items = NULL;
	
}

int tags_dumpf(const struct Tags* const tags, struct FStream* stream) {
	
	for (size_t index = 0; index < tags->offset; index++) {
		struct Tag* tag = &tags->items[index];
		
		if (fstream_write(stream, HASHTAG, strlen(HASHTAG)) == -1) {
			return -1;
		}
		
		const char* const name = tag_stringify(tag->type);
		
		if (fstream_write(stream, name, strlen(name)) == -1) {
			return -1;
		}
		
		if (tag->value == NULL) {
			for (size_t index = 0; index < tag->attributes.offset; index++) {
				if (index == 0) {
					if (fstream_write(stream, COLON, strlen(COLON)) == -1) {
						return -1;
					}
				} else {
					if (fstream_write(stream, COMMA, strlen(COMMA)) == -1) {
						return -1;
					}
				}
				
				const struct Attribute* const attribute = &tag->attributes.items[index];
				
				if (fstream_write(stream, attribute->key, strlen(attribute->key)) == -1) {
					return -1;
				}
				
				if (fstream_write(stream, EQUAL, strlen(EQUAL)) == -1) {
					return -1;
				}
				
				if (attribute->is_quoted) {
					if (fstream_write(stream, QUOTATION_MARK, strlen(QUOTATION_MARK)) == -1) {
						return -1;
					}
				}
				
				if (strcmp(attribute->key, "URI") == 0) {
					char value[strlen(attribute->value) + 1];
					strcpy(value, attribute->value);
					
					for (size_t index = 0; index < strlen(value); index++) {
						char* ch = &value[index];
						
						if (*ch == *BACKSLASH) {
							*ch = *SLASH;
						}
					}
					
					if (fstream_write(stream, value, strlen(value)) == -1) {
						return -1;
					}
				} else {
					if (fstream_write(stream, attribute->value, strlen(attribute->value)) == -1) {
						return -1;
					}
				}
				
				if (attribute->is_quoted) {
					if (fstream_write(stream, QUOTATION_MARK, strlen(QUOTATION_MARK)) == -1) {
						return -1;
					}
				}
			}
		} else {
			if (fstream_write(stream, COLON, strlen(COLON)) == -1) {
				return -1;
			}
			
			if (fstream_write(stream, tag->value, strlen(tag->value)) == -1) {
				return -1;
			}
		}
		
		if (tag->uri != NULL) {
			if (fstream_write(stream, LF, strlen(LF)) == -1) {
				return -1;
			}
			
			char uri[strlen(tag->uri) + 1];
			strcpy(uri, tag->uri);
			
			for (size_t index = 0; index < strlen(uri); index++) {
				char* ch = &uri[index];
				
				if (*ch == *BACKSLASH) {
					*ch = *SLASH;
				}
			}
			
			if (fstream_write(stream, uri, strlen(uri)) == -1) {
				return -1;
			}
		}
		
		if (fstream_write(stream, LF, strlen(LF)) == -1) {
			return -1;
		}
	}
	
	return 0;
	
}

struct Attribute* attributes_get(const struct Attributes* attributes, const char* key) {
	
	for (size_t index = 0; index < attributes->offset; index++) {
		 struct Attribute* const attribute = &attributes->items[index];
		
		if (strcmp(attribute->key, key) == 0) {
			return attribute;
		}
	}
	
	return NULL;
	
}

int attribute_set_value(struct Attribute* attribute, const char* const value) {
	
	free(attribute->value);
	
	attribute->value = malloc(strlen(value) + 1);
	
	if (attribute->value == NULL) {
		return 0;
	}
	
	strcpy(attribute->value, value);
	
	return 1;
	
}

int tag_set_value(struct Tag* tag, const char* const value) {
	
	free(tag->value);
	
	tag->value = malloc(strlen(value) + 1);
	
	if (tag->value == NULL) {
		return 0;
	}
	
	strcpy(tag->value, value);
	
	return 1;
	
}

int tag_set_uri(struct Tag* tag, const char* const value) {
	
	free(tag->uri);
	
	tag->uri = malloc(strlen(value) + 1);
	
	if (tag->uri == NULL) {
		return 0;
	}
	
	strcpy(tag->uri, value);
	
	return 1;
	
}
