static const char HOTMART_CLUB_SUFFIX[] = ".club.hotmart.com";
static const char HOTMART_EMBED_PAGE_PREFIX[] = "/t/page-embed/";

struct Credentials {
	char* username;
	char* access_token;
	char* refresh_token;
	long long expires_in;
};

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


int authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
);

int get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
);

int get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
);

int get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
);

void credentials_free(struct Credentials* obj);

#pragma once