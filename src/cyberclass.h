#include "credentials.h"
#include "resources.h"

int cyberclass_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
);

int cyberclass_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
);

int cyberclass_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
);

int cyberclass_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
);

int cyberclass_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
);

#pragma once
