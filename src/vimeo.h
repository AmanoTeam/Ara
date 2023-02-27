#include "resources.h"

int vimeo_matches(const char* const url);

int vimeo_parse(
	const char* const url,
	const struct Resource* const resource,
	const struct Page* const page,
	struct Media* const media,
	const char* const referer
);
