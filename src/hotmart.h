#include "credentials.h"
#include "resources.h"

static const char HOTMART_CLUB_SUFFIX[] = ".club.hotmart.com";
static const char HOTMART_EMBED_PAGE_PREFIX[] = "/t/page-embed/";

int hotmart_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
);

int hotmart_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
);

int hotmart_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
);

int hotmart_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
);

int hotmart_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
);

#pragma once
