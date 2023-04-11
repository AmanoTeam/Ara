#include "credentials.h"
#include "resources.h"

int kiwify_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
);

int kiwify_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
);

int kiwify_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
);

int kiwify_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
);

int kiwify_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
);

#pragma once
