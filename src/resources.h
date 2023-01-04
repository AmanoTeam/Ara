#include <stdlib.h>

enum MediaType {
	MEDIA_SINGLE,
	MEDIA_M3U8
};

struct AudioStream {
	char* id;
	char* filename;
	char* short_filename;
	char* url;
};

struct VideoStream {
	char* id;
	char* filename;
	char* short_filename;
	char* url;
};

struct Media {
	enum MediaType type;
	struct AudioStream audio;
	struct VideoStream video;
	char* path;
};

struct Medias {
	size_t offset;
	size_t size;
	struct Media* items;
};

struct Attachment {
	char* id;
	char* filename;
	char* short_filename;
	char* url;
	char* path;
};

struct Attachments {
	size_t offset;
	size_t size;
	struct Attachment* items;
};

struct HTMLDocument {
	char* id;
	char* filename;
	char* short_filename;
	char* content;
	char* path;
};

struct Page {
	char* id;
	char* name;
	char* dirname;
	char* short_dirname;
	struct HTMLDocument document;
	struct Medias medias;
	struct Attachments attachments;
	int is_locked;
	char* path;
};

struct Pages {
	size_t offset;
	size_t size;
	struct Page* items;
};

struct Module {
	char* id;
	char* name;
	char* dirname;
	char* short_dirname;
	int is_locked;
	struct Attachments attachments;
	struct Pages pages;
	char* path;
};

struct Modules {
	size_t offset;
	size_t size;
	struct Module* items;
};

struct Qualification {
	char* id;
	char* name;
	char* dirname;
	char* short_dirname;
};

struct Resource {
	char* id;
	char* name;
	char* dirname;
	char* short_dirname;
	char* url;
	struct Qualification qualification;
	struct Modules modules;
	char* path;
};

struct Resources {
	size_t offset;
	size_t size;
	struct Resource* items;
};

#pragma once
