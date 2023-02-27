#include "resources.h"

int panda_matches(const char* const url);

int panda_parse(
	const char* const url,
	const struct Resource* const resource,
	const struct Page* const page,
	struct Media* const media,
	const char* const referer
);
