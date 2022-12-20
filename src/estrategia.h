#include "credentials.h"
#include "resources.h"

int estrategia_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
);

int estrategia_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
);

int estrategia_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
);

int estrategia_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
);

#pragma once
