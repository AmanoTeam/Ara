#include <stdio.h>

#include "fstream.h"

#define M3U8ERR_SUCCESS 0 /* Success */
#define M3U8ERR_MEMORY_ALLOCATE_FAILURE -1 /* Could not allocate memory */
#define M3U8ERR_ATTRIBUTE_NAME_INVALID -2 /* The name of this M3U8 attribute was not recognized */

#define M3U8ERR_TAG_NAME_INVALID -3 /* The name of this M3U8 tag was not recognized */
#define M3U8ERR_TAG_DUPLICATE -4 /* This M3U8 tag cannot be specified multiple times in the same playlist */
#define M3U8ERR_TAG_MISSING_ATTRIBUTES -5 /* This M3U8 tag requires a list of attributes to be supplied */
#define M3U8ERR_TAG_MISSING_VALUE -6 /* This M3U8 tag requires a single-value option to be supplied */
#define M3U8ERR_TAG_MISSING_ITEMS -7 /* This M3U8 tag requires a list of items to be supplied */
#define M3U8ERR_TAG_TRAILING_OPTIONS -8 /* This M3U8 tag does not require any value to be supplied, but trailing options were found */

#define M3U8ERR_ATTRIBUTE_MISSING_VALUE -9 /* An M3U8 attribute requires a value to be supplied */
#define M3U8ERR_ATTRIBUTE_MISSING_NAME -10 /* An M3U8 attribute requires a key to be supplied */
#define M3U8ERR_ATTRIBUTE_EMPTY -11 /* An M3U8 tag must not contain empty attributes */
#define M3U8ERR_ATTRIBUTE_DUPLICATE -12 /* There must not have multiple attributes with the same name within the same tag */

#define M3U8ERR_ITEM_EMPTY -13 /* This M3U8 tag must not contain empty items */

#define M3U8ERR_PLAYLIST_INVALID -14 /* This M3U8 playlist is invalid */
#define M3U8ERR_PLAYLIST_LINE_TOO_LONG -15 /* This M3U8 playlist contains a line that is too long */
#define M3U8ERR_PLAYLIST_LINE_UNTERMINATED -16 /* This M3U8 playlist contains a line that was not terminated */

#define M3U8ERR_ATTRIBUTE_NOT_FOUND -17 /* This M3U8 attribute does not exists within the specified M3U8 tag */

#define M3U8ERR_IO_WRITE_FAILURE -18 /* Could not write data to output file */

enum M3U8TagVType {
	M3U8TAGV_NONE, /* This M3U8 tag does not expect any value to be supplied */
	M3U8TAGV_ATTRIBUTES_LIST,  /* This M3U8 tag expects a list of attributes to be supplied */
	M3U8TAGV_ITEMS_LIST, /* This M3U8 tag expects a list of items to be supplied */
	M3U8TAGV_SINGLE_VALUE /* This M3U8 tag expects a single-value option to be supplied */
};

enum M3U8TagType {
	EXTM3U = 0x00000001,
	EXT_X_VERSION = 0x00000002,
	EXTINF = 0x00000004,
	EXT_X_BYTERANGE = 0x00000010,
	EXT_X_DISCONTINUITY = 0x00000020,
	EXT_X_KEY = 0x00000040,
	EXT_X_MAP = 0x00000080,
	EXT_X_PROGRAM_DATE_TIME = 0x00000100,
	EXT_X_DATERANGE = 0x00000200,
	EXT_X_TARGETDURATION = 0x00000400,
	EXT_X_MEDIA_SEQUENCE = 0x00000800,
	EXT_X_DISCONTINUITY_SEQUENCE = 0x00001000,
	EXT_X_ENDLIST = 0x00002000,
	EXT_X_PLAYLIST_TYPE = 0x00004000,
	EXT_X_I_FRAMES_ONLY = 0x00008000,
	EXT_X_MEDIA = 0x00010000,
	EXT_X_STREAM_INF = 0x00020000,
	EXT_X_I_FRAME_STREAM_INF = 0x00040000,
	EXT_X_SESSION_DATA = 0x00080000,
	EXT_X_SESSION_KEY = 0x00100000,
	EXT_X_INDEPENDENT_SEGMENTS = 0x00200000,
	EXT_X_START = 0x00400000
};

struct M3U8Attribute {
	char* key;
	char* value;
	int is_quoted;
};

struct M3U8Attributes {
	size_t offset;
	size_t size;
	struct M3U8Attribute* items;
};

struct M3U8Item {
	char* value;
};

struct M3U8Items {
	size_t offset;
	size_t size;
	struct M3U8Item* items;
};

struct M3U8Tag {
	enum M3U8TagType type;
	enum M3U8TagVType vtype;
	struct M3U8Attributes attributes;
	struct M3U8Items items;
	char* value;
	char* uri;
};

struct M3U8Tags {
	size_t offset;
	size_t size;
	struct M3U8Tag* items;
};

struct M3U8Playlist {
	struct M3U8Tags tags;
};

int m3u8_parse(struct M3U8Playlist* const playlist, const char* const s);
void m3u8_free(struct M3U8Playlist* const playlist);

int m3u8_dumpf(const struct M3U8Playlist* const playlist, struct FStream* const stream);

const char* strm3u8err(const int code);

enum M3U8TagSetType {
	M3U8TAG_SET_URI,
	M3U8TAG_SET_VALUE,
	M3U8TAG_SET_ATTRIBUTES,
	M3U8TAG_SET_ITEMS
};

int m3u8tag_set(
	struct M3U8Tag* const tag,
	const enum M3U8TagSetType what,
	const void* const value
);

struct M3U8Attribute* m3u8tag_getattr(
	const struct M3U8Tag* const tag,
	const char* const key
);

int m3u8tag_setattr(
	struct M3U8Tag* const tag,
	const char* const key,
	const char* const value
);
