#include "credentials.h"
#include "resources.h"

int estrategia_vestibulares_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
);

int estrategia_vestibulares_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
);

int estrategia_vestibulares_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
);

int estrategia_vestibulares_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
);

int estrategia_vestibulares_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
);

#pragma once
