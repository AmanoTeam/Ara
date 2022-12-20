#include <stdlib.h>

enum MediaType {
	MEDIA_SINGLE,
	MEDIA_M3U8
};

struct AudioStream {
	char* filename;
	char* url;
};

struct VideoStream {
	char* filename;
	char* url;
};


struct Media {
	enum MediaType type;
	struct AudioStream audio;
	struct VideoStream video;
};

struct Medias {
	size_t offset;
	size_t size;
	struct Media* items;
};

struct Attachment {
	char* filename;
	char* url;
};

struct Attachments {
	size_t offset;
	size_t size;
	struct Attachment* items;
};

struct HTMLDocument {
	char* filename;
	char* content;
};

struct Page {
	char* id;
	char* name;
	struct HTMLDocument document;
	struct Medias medias;
	struct Attachments attachments;
	int is_locked;
};

struct Pages {
	size_t offset;
	size_t size;
	struct Page* items;
};

struct Module {
	char* id;
	char* name;
	int is_locked;
	struct Attachment attachment;
	struct Pages pages;
};

struct Modules {
	size_t offset;
	size_t size;
	struct Module* items;
};

struct Resource {
	char* id;
	char* name;
	struct Modules modules;
};

struct Resources {
	size_t offset;
	size_t size;
	struct Resource* items;
};

#pragma once
