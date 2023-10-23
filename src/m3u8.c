#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "m3u8.h"
#include "errors.h"
#include "symbols.h"
#include "fstream.h"
#include "readlines.h"

/*
static const char s[] = 
	"#EXTM3U\n"
	"#EXTINF:<duration>,[<title>]\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1, \\\nBANDWIDTH=288000,RESOLUTION=256x144\n"
	"low/skate_phantom_flex_4k_228_144p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=582000,RESOLUTION=426x240\n"
	"medium/skate_phantom_flex_4k_452_240p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1018000,RESOLUTION=640x360\n"
	"high/skate_phantom_flex_4k_788_360p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1366000,RESOLUTION=854x480\n"
	"veryhigh/skate_phantom_flex_4k_1056_480p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=2742000,CODECS=\"avc1.4d001f,mp4a.40.2\",RESOLUTION=1280x720\n"
	"hdready/skate_phantom_flex_4k_2112_720p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=5400000,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=1920x1080\n"
	"fullhd/skate_phantom_flex_4k_4160_1080p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=7556000,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=2560x1440\n"
	"2k/skate_phantom_flex_4k_5816_1440p.m3u8\n"
	"#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=10768000,CODECS=\"avc1.640028,mp4a.40.2\",RESOLUTION=3840x2160\n"
	"4k/skate_phantom_flex_4k_8288_2160p.m3u8\n";
*/

#define M3U8_MAX_LINE (1024 * 10) // 10 KiB should be enough

#define M3U8_PLAYLIST_TAG 1
#define M3U8_PLAYLIST_COMMENT 2
#define M3U8_PLAYLIST_URI 3

static const enum M3U8TagType M3U8_TAGS[] = {
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

static const char* m3u8tag_stringify(const enum M3U8TagType type) {
	
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

static enum M3U8TagType m3u8tag_unstringify(const char* const name) {
	
	for (size_t index = 0; index < sizeof(M3U8_TAGS) / sizeof(*M3U8_TAGS); index++) {
		const enum M3U8TagType tag_type = M3U8_TAGS[index];
		const char* const tag_name = m3u8tag_stringify(tag_type);
		
		if (strcmp(name, tag_name) == 0) {
			return tag_type;
		}
	}
	
	return (enum M3U8TagType) 0;
	
}

static int namesafe(const char* const s) {
	
	for (size_t index = 0; index < strlen(s); index++) {
		const unsigned char ch = s[index];
		
		const int safe = (isdigit(ch) || isupper(ch) || ch == *HYPHEN);
		
		if (!safe) {
			return safe;
		}
	}
	
	return 1;
	
}

static enum M3U8TagVType m3u8tag_get_vtype(const struct M3U8Tag tag) {
	
	switch (tag.type) {
		case EXTM3U:
		case EXT_X_DISCONTINUITY:
		case EXT_X_ENDLIST:
		case EXT_X_I_FRAMES_ONLY:
		case EXT_X_INDEPENDENT_SEGMENTS:
			return M3U8TAGV_NONE;
		case EXT_X_VERSION:
		case EXT_X_BYTERANGE:
		case EXT_X_PROGRAM_DATE_TIME:
		case EXT_X_TARGETDURATION:
		case EXT_X_MEDIA_SEQUENCE:
		case EXT_X_DISCONTINUITY_SEQUENCE:
		case EXT_X_PLAYLIST_TYPE:
			return M3U8TAGV_SINGLE_VALUE;
		case EXT_X_KEY:
		case EXT_X_MAP:
		case EXT_X_DATERANGE:
		case EXT_X_MEDIA:
		case EXT_X_STREAM_INF:
		case EXT_X_I_FRAME_STREAM_INF:
		case EXT_X_SESSION_DATA:
		case EXT_X_SESSION_KEY:
		case EXT_X_START:
			return M3U8TAGV_ATTRIBUTES_LIST;
		case EXTINF:
			return M3U8TAGV_ITEMS_LIST;
	}
	
}

int m3u8_parse(struct M3U8Playlist* const playlist, const char* const s) {
	
	struct ReadLines readlines = {0};
	readlines_init(&readlines, s);
	
	struct Line current_line = {0};
	
	enum M3U8TagType tags = 0;
	
	char line[M3U8_MAX_LINE];
	
	while (1) {
		if (readlines_next(&readlines, &current_line) == NULL) {
			break;
		}
		
		if ((current_line.size + 1) > sizeof(line)) {
			return M3U8ERR_PLAYLIST_LINE_TOO_LONG;
		}
		
		memcpy(line, current_line.begin, current_line.size);
		line[current_line.size] = '\0';
		
		struct M3U8Tag tag = {0};
		int type = 0;
		
		if (*line == *HASHTAG) {
			type = (memcmp(line + 1, "EXT", 3) == 0) ? M3U8_PLAYLIST_TAG : M3U8_PLAYLIST_COMMENT;
		} else {
			type = M3U8_PLAYLIST_URI;
		}
		
		// RFC 8216 states that the first line in the playlist must be the 'EXTM3U' tag; neither comments nor URIs are allowed before this
		if (current_line.index == 0 && type != M3U8_PLAYLIST_TAG) {
			return M3U8ERR_PLAYLIST_INVALID;
		}
		
		switch (type) {
			case M3U8_PLAYLIST_TAG: {
				/*
				In RFC 8216, Section 8.6, it is stated that M3U8 tags can continue on the next line by
				specifying a backslash ('\') at the end of the line
				*/
				while (current_line.begin[current_line.size - 1] == *BACKSLASH) {
					if (readlines_next(&readlines, &current_line) == NULL) {
						return M3U8ERR_PLAYLIST_LINE_UNTERMINATED;
					}
					
					current_line.index--;
					
					char* position = line + (strlen(line) - 1);
					
					// Remove the backslash from the line
					*position = '\0';
					
					// Remove trailing whitespaces
					while (position != line) {
						position--;
						
						if (!isspace(*position)) {
							break;
						}
						
						*position = '\0';
					}
					
					position++;
					
					if ((strlen(line) + current_line.size + 1) > sizeof(line)) {
						return M3U8ERR_PLAYLIST_LINE_TOO_LONG;
					}
					
					memcpy(position, current_line.begin, current_line.size);
					position[current_line.size] = '\0';
				}
				
				const char* start = line + 1;
				const char* end = strstr(start, COLON);
				
				if (end == NULL) {
					end = strchr(start, '\0');
				}
				
				const size_t name_size = (size_t) (end - start);
				
				char name[name_size + 1];
				memcpy(name, start, name_size);
				name[name_size] = '\0';
				
				// RFC 8216 states that an tag name is only valid if it contains characters from the set [A-Z], [0-9], and '-'
				if (!namesafe(name)) {
					return M3U8ERR_TAG_NAME_INVALID;
				}
				
				tag.type = m3u8tag_unstringify(name);
				
				if (tag.type == 0) {
					return M3U8ERR_TAG_NAME_INVALID;
				}
				
				// RFC 8216 states that the EXTM3U tag must be the first line of every Media Playlist and Master Playlist
				if (current_line.index == 0 && tag.type != EXTM3U) {
					return M3U8ERR_PLAYLIST_INVALID;
				}
				
				tag.vtype = m3u8tag_get_vtype(tag);
				
				const char* const lend = strchr(line, '\0');
				
				/*
				According to RFC 8216, an M3U8 tag may or may not require a value to be supplied after it.
				For those tags for which a value is required, there are basically three types of values accepted:
				
				* A list of key=value pairs delimited with a comma (e.g., "X-EXT-EXAMPLE:key=value,anotherkey=anothervalue").
				* A list of items delimited with a comma (e.g., "X-EXT-EXAMPLE:9999,Some string").
				* A single-value one (e.g., "X-EXT-EXAMPLE:value").
				*/
				switch (tag.vtype) {
					case M3U8TAGV_NONE: {
						if (end != lend) {
							return M3U8ERR_TAG_TRAILING_OPTIONS;
						}
						
						break;
					}
					case M3U8TAGV_ATTRIBUTES_LIST: {
						if (end == lend) {
							return M3U8ERR_TAG_MISSING_ATTRIBUTES;
						}
						
						end++;
						
						start = end;
						end = strstr(start, COMMA);
						
						while (1) {
							if (end == NULL) {
								end = lend;
							}
							
							const size_t attribute_size = (size_t) (end - start);
							
							if (attribute_size < 1) {
								return M3U8ERR_ATTRIBUTE_EMPTY;
							}
							
							char attribute[attribute_size + 1];
							memcpy(attribute, start, attribute_size);
							attribute[attribute_size] = '\0';
							
							const char* separator = strstr(start, EQUAL);
							
							if (separator == NULL) {
								return M3U8ERR_ATTRIBUTE_MISSING_VALUE;
							}
							
							struct M3U8Attribute attr = {0};
							
							const size_t key_size = (size_t) (separator - start);
							
							if (key_size < 1) {
								return M3U8ERR_ATTRIBUTE_MISSING_NAME;
							}
							
							attr.key = malloc(key_size + 1);
							
							if (attr.key == NULL) {
								return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
							}
							
							memcpy(attr.key, start, key_size);
							attr.key[key_size] = '\0';
							
							// RFC 8216 states that an attribute name is only valid if it contains characters from the set [A-Z], [0-9], and '-'
							if (!namesafe(attr.key)) {
								free(attr.key);
								return M3U8ERR_ATTRIBUTE_NAME_INVALID;
							}
							
							// RFC 8216 states that there must not have duplicate attributes within the same tag
							for (size_t index = 0; index < tag.attributes.offset; index++) {
								const struct M3U8Attribute* const attribute = &tag.attributes.items[index];
								
								const int already_exists = (strcmp(attribute->key, attr.key) == 0);
								
								if (already_exists) {
									free(attr.key);
									return M3U8ERR_ATTRIBUTE_DUPLICATE;
								}
							}
							
							if (separator != end) {
								separator++;
							}
							
							if (*separator == *QUOTATION_MARK) {
								separator++;
								
								const char* value_end = strstr(separator, QUOTATION_MARK);
								
								if (value_end == NULL) {
									return UERR_M3U8_UNTERMINATED_STRING_LITERAL;
								}
								
								end = value_end;
								attr.is_quoted = 1;
							}
							
							const size_t value_size = (size_t) (separator == end ? 0 : end - separator);
							
							if (value_size < 1) {
								return M3U8ERR_ATTRIBUTE_MISSING_VALUE;
							}
							
							attr.value = malloc(value_size + 1);
							
							if (attr.value == NULL) {
								return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
							}
							
							memcpy(attr.value, separator, value_size);
							attr.value[value_size] = '\0';
							
							const size_t size = tag.attributes.size + sizeof(*tag.attributes.items) * 1;
							struct M3U8Attribute* items = realloc(tag.attributes.items, size);
							
							if (items == NULL) {
								return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
							}
							
							tag.attributes.items = items;
							tag.attributes.size = size;
							
							tag.attributes.items[tag.attributes.offset++] = attr;
							
							if (end == lend) {
								break;
							}
							
							start = end;
							start++;
							
							if (*start == *COMMA) {
								start++;
							}
							
							end = strstr(start, COMMA);
						}
						
						break;
					}
					case M3U8TAGV_ITEMS_LIST: {
						if (end == lend) {
							return M3U8ERR_TAG_MISSING_ITEMS;
						}
						
						end++;
						
						start = end;
						end = strstr(start, COMMA);
						
						while (1) {
							if (end == NULL) {
								end = lend;
							}
							
							const size_t value_size = (size_t) (end - start);
							/*
							if (value_size < 1) {
								return M3U8ERR_ITEM_EMPTY;
							}
							*/
							
							if (value_size > 0) {
								struct M3U8Item item = {0};
								
								item.value = malloc(value_size + 1);
								
								if (item.value == NULL) {
									return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
								}
								
								memcpy(item.value, start, value_size);
								item.value[value_size] = '\0';
								
								const size_t size = tag.items.size + sizeof(*tag.items.items) * 1;
								struct M3U8Item* items = realloc(tag.items.items, size);
								
								if (items == NULL) {
									return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
								}
								
								tag.items.items = items;
								tag.items.size = size;
								
								tag.items.items[tag.items.offset++] = item;
							}
							
							if (end == lend) {
								break;
							}
							
							start = end;
							start++;
							
							if (*start == *COMMA) {
								start++;
							}
							
							end = strstr(start, COMMA);
						}
						
						break;
					}
					case M3U8TAGV_SINGLE_VALUE: {
						if (end == lend) {
							return M3U8ERR_TAG_MISSING_VALUE;
						}
						
						end++;
						
						start = end;
						end = lend;
						
						const size_t value_size = (size_t) (end - start);
						
						if (value_size < 1) {
							return M3U8ERR_TAG_MISSING_VALUE;
						}
						
						tag.value = malloc(value_size + 1);
						
						if (tag.value == NULL) {
							return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
						}
						
						memcpy(tag.value, start, value_size);
						tag.value[value_size] = '\0';
						
						break;
					}
				}
				
				/*
				There is nothing in RFC 8216 explicitly saying that these tags cannot appear multiple times in the same playlist file,
				but based on their common usage, I judged that they don't need to appear multiple times or would not make sense if they were specified multiple times.
				*/
				if ((tag.type == EXT_X_VERSION || tag.type == EXT_X_TARGETDURATION
					|| tag.type ==  EXT_X_MEDIA_SEQUENCE || tag.type == EXT_X_ENDLIST
					|| tag.type ==  EXTM3U || tag.type ==  EXT_X_PLAYLIST_TYPE
					|| tag.type == EXT_X_I_FRAMES_ONLY) && (tags & tag.type) != 0) {
					return M3U8ERR_TAG_DUPLICATE;
				}
				
				tags |= tag.type;
				
				const size_t size = playlist->tags.size + sizeof(*playlist->tags.items) * 1;
				struct M3U8Tag* items = (struct M3U8Tag*) realloc(playlist->tags.items, size);
				
				if (items == NULL) {
					return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				playlist->tags.items = items;
				playlist->tags.size = size;
				
				playlist->tags.items[playlist->tags.offset++] = tag;
				
				break;
			}
			case M3U8_PLAYLIST_COMMENT: {
				break; // Does anyone even read these comments?!
			}
			case M3U8_PLAYLIST_URI: {
				struct M3U8Tag* const tag = &playlist->tags.items[playlist->tags.offset - 1];
				
				// Only the EXT-X-KEY, EXT-X-STREAM-INF and EXTINF tags are allowed to have an URI
				if (!(tag->type == EXT_X_KEY || tag->type == EXT_X_STREAM_INF || tag->type == EXTINF)) {
					return M3U8ERR_PLAYLIST_INVALID;
				}
				
				tag->uri = malloc(sizeof(line));
				
				if (tag->uri == NULL) {
					return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(tag->uri, line);
			}
		}
	}
	
	return M3U8ERR_SUCCESS;
	
}

void m3u8_free(struct M3U8Playlist* const playlist) {

	for (size_t index = 0; index < playlist->tags.offset; index++) {
		struct M3U8Tag* const tag = &playlist->tags.items[index];
		
		for (size_t index = 0; index < tag->attributes.offset; index++) {
			struct M3U8Attribute* const attribute = &tag->attributes.items[index];
			
			free(attribute->key);
			attribute->key = NULL;
			
			free(attribute->value);
			attribute->value = NULL;
		}
		
		tag->attributes.offset = 0;
		tag->attributes.size = 0;
		free(tag->attributes.items);
		tag->attributes.items = NULL;
		
		for (size_t index = 0; index < tag->items.offset; index++) {
			struct M3U8Item* const item = &tag->items.items[index];
			
			free(item->value);
			item->value = NULL;
		}
		
		tag->items.offset = 0;
		tag->items.size = 0;
		free(tag->items.items);
		tag->items.items = NULL;
		
		free(tag->uri);
		tag->uri = NULL;
		
		free(tag->value);
		tag->value = NULL;
	}
	
	playlist->tags.offset = 0;
	playlist->tags.size = 0;
	free(playlist->tags.items);
	playlist->tags.items = NULL;
	
}

int m3u8_dumpf(const struct M3U8Playlist* const playlist, struct FStream* const stream) {
	
	for (size_t index = 0; index < playlist->tags.offset; index++) {
		const struct M3U8Tag* const tag = &playlist->tags.items[index];
		
		if (fstream_write(stream, HASHTAG, strlen(HASHTAG)) == -1) {
			return -1;
		}
		
		const char* const name = m3u8tag_stringify(tag->type);
		
		if (fstream_write(stream, name, strlen(name)) == -1) {
			return -1;
		}
		
		switch (tag->vtype) {
			case M3U8TAGV_NONE: {
				break; // Nothing to do
			}
			case M3U8TAGV_ATTRIBUTES_LIST: {
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
					
					const struct M3U8Attribute* const attribute = &tag->attributes.items[index];
					
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
				
				break;
			}
			case M3U8TAGV_ITEMS_LIST: {
				for (size_t index = 0; index < tag->items.offset; index++) {
					if (index == 0) {
						if (fstream_write(stream, COLON, strlen(COLON)) == -1) {
							return -1;
						}
					} else {
						if (fstream_write(stream, COMMA, strlen(COMMA)) == -1) {
							return -1;
						}
					}
					
					const struct M3U8Item* const item = &tag->items.items[index];
					
					if (fstream_write(stream, item->value, strlen(item->value)) == -1) {
						return -1;
					}
				}
				
				break;
			}
			case M3U8TAGV_SINGLE_VALUE: {
				if (fstream_write(stream, COLON, strlen(COLON)) == -1) {
					return -1;
				}
				
				if (fstream_write(stream, tag->value, strlen(tag->value)) == -1) {
					return -1;
				}
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

const char* strm3u8err(const int code) {
	
	switch (code) {
		case M3U8ERR_SUCCESS:
			return "Success";
		case M3U8ERR_MEMORY_ALLOCATE_FAILURE:
			return "Could not allocate memory";
		case M3U8ERR_ATTRIBUTE_NAME_INVALID:
			return "The name of this M3U8 attribute was not recognized";
		case M3U8ERR_TAG_NAME_INVALID:
			return "The name of this M3U8 tag was not recognized";
		case M3U8ERR_TAG_DUPLICATE:
			return "This M3U8 tag cannot be specified multiple times in the same playlist";
		case M3U8ERR_TAG_MISSING_ATTRIBUTES:
			return "This M3U8 tag requires a list of attributes to be supplied";
		case M3U8ERR_TAG_MISSING_VALUE:
			return "This M3U8 tag requires a single-value option to be supplied";
		case M3U8ERR_TAG_MISSING_ITEMS:
			return "This M3U8 tag requires a list of items to be supplied";
		case M3U8ERR_TAG_TRAILING_OPTIONS:
			return "This M3U8 tag does not require any value to be supplied, but trailing options were found";
		case M3U8ERR_ATTRIBUTE_MISSING_VALUE:
			return "An M3U8 attribute requires a value to be supplied";
		case M3U8ERR_ATTRIBUTE_MISSING_NAME:
			return "An M3U8 attribute requires a key to be supplied";
		case M3U8ERR_ATTRIBUTE_EMPTY:
			return "An M3U8 tag must not contain empty attributes";
		case M3U8ERR_ATTRIBUTE_DUPLICATE:
			return "There must not have multiple attributes with the same name within the same tag";
		case M3U8ERR_ITEM_EMPTY:
			return "This M3U8 tag must not contain empty items";
		case M3U8ERR_PLAYLIST_INVALID:
			return "This M3U8 playlist is invalid";
		case M3U8ERR_PLAYLIST_LINE_TOO_LONG:
			return "This M3U8 playlist contains a line that is too long";
		case M3U8ERR_PLAYLIST_LINE_UNTERMINATED:
			return "This M3U8 playlist contains a line that was not terminated";
		case M3U8ERR_ATTRIBUTE_NOT_FOUND:
			return "This M3U8 attribute does not exists within the specified M3U8 tag";
		default:
			return "Unknown error";
	}
	
}

struct M3U8Attribute* m3u8tag_getattr(
	const struct M3U8Tag* const tag,
	const char* const key
) {
	
	for (size_t index = 0; index < tag->attributes.offset; index++) {
		struct M3U8Attribute* const attribute = &tag->attributes.items[index];
		
		if (strcmp(attribute->key, key) == 0) {
			return attribute;
		}
	}
	
	return NULL;
	
}

int m3u8tag_setattr(
	struct M3U8Tag* const tag,
	const char* const key,
	const char* const value
) {
	
	struct M3U8Attribute* const attribute = m3u8tag_getattr(tag, key);
	
	if (attribute == NULL) {
		return M3U8ERR_ATTRIBUTE_NOT_FOUND;
	}
	
	free(attribute->value);
	
	attribute->value = malloc(strlen(value) + 1);
	
	if (attribute->value == NULL) {
		return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(attribute->value, value);
	
	return M3U8ERR_SUCCESS;
	
}

int m3u8tag_set(
	struct M3U8Tag* const tag,
	const enum M3U8TagSetType what,
	const void* const value
) {
	
	switch (what) {
		case M3U8TAG_SET_URI: {
			free(tag->uri);
			
			tag->uri = malloc(strlen(value) + 1);
			
			if (tag->uri == NULL) {
				return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(tag->uri, value);
			
			break;
		}
		case M3U8TAG_SET_VALUE: {
			free(tag->value);
			
			tag->value = malloc(strlen(value) + 1);
			
			if (tag->value == NULL) {
				return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			strcpy(tag->value, value);
			
			break;
		}
		case M3U8TAG_SET_ATTRIBUTES: {
			const struct M3U8Attributes* const attributes = (const struct M3U8Attributes* const) value;
			
			tag->attributes.size = attributes->size;
			tag->attributes.items = malloc(tag->attributes.size);
			
			if (tag->attributes.items == NULL) {
				return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			for (size_t index = 0; index < attributes->offset; index++) {
				const struct M3U8Attribute* const source = &attributes->items[index];
				
				struct M3U8Attribute destination = {
					.key = malloc(strlen(source->key) + 1),
					.value = malloc(strlen(source->value) + 1),
					.is_quoted = source->is_quoted
				};
				
				if (destination.key == NULL) {
					return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				if (destination.value == NULL) {
					return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(destination.key, source->key);
				strcpy(destination.value, source->value);
				
				tag->attributes.items[tag->attributes.offset++] = destination;
			}
			
			break;
		}
		case M3U8TAG_SET_ITEMS: {
			const struct M3U8Items* const items = (const struct M3U8Items* const) value;
			
			tag->items.size = items->size;
			tag->items.items = malloc(tag->items.size);
			
			if (tag->items.items == NULL) {
				return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			for (size_t index = 0; index < items->offset; index++) {
				const struct M3U8Item* const source = &items->items[index];
				
				struct M3U8Item destination = {
					.value = malloc(strlen(source->value) + 1)
				};
				
				if (destination.value == NULL) {
					return M3U8ERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				strcpy(destination.value, source->value);
				
				tag->items.items[tag->items.offset++] = destination;
			}
			
			break;
		}
	}
	
	return M3U8ERR_SUCCESS;
	
}

/*
int main() {
	
	struct M3U8Playlist playlist = {0};
	
	int c = m3u8_parse(&playlist, s);
	printf("%i\n", c);
	
	if (c != M3U8ERR_SUCCESS) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return UERR_FAILURE;
	}
	struct FStream* const stream = fstream_open("/sdcard/x.m3u8", FSTREAM_WRITE);
	printf("%i\n", m3u8_dumpf(&playlist, stream));
	fstream_close(stream);
	
}
*/