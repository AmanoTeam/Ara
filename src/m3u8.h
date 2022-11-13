#include <stdio.h>

#include "fstream.h"

enum Type {
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

struct Attribute {
	char* key;
	char* value;
	int is_quoted;
};

struct Attributes {
	size_t offset;
	size_t size;
	struct Attribute* items;
};

struct Tag {
	enum Type type;
	struct Attributes attributes;
	char* value;
	char* uri;
};

struct Tags {
	size_t offset;
	size_t size;
	struct Tag* items;
};

int m3u8_parse(struct Tags* tags, const char* const s);
void m3u8_free(struct Tags* tags);

int tags_dumpf(const struct Tags* const tags, struct FStream* stream);

const char* tag_stringify(const enum Type type);
int tag_set_value(struct Tag* tag, const char* const value);
int tag_set_uri(struct Tag* tag, const char* const value);

struct Attribute* attributes_get(const struct Attributes* attributes, const char* key);
int attribute_set_value(struct Attribute* attribute, const char* const value);