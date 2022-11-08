#include <stdlib.h>

struct Credentials {
	char* username;
	char* access_token;
	char* refresh_token;
	int expires_in;
};

enum MediaType {
	MEDIA_SINGLE,
	MEDIA_M3U8
};

struct Media {
	enum MediaType type;
	char* filename;
	char* url;
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
	int is_locked;
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

struct String {
	char *s;
	size_t slength;
};

void string_free(struct String* obj);
void credentials_free(struct Credentials* obj);

#pragma once