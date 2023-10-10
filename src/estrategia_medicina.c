#include "credentials.h"
#include "resources.h"
#include "estrategia.h"

static const char ESTRATEGIA_VERTICAL[] = "medicina";

int estrategia_medicina_authorize(
	const char* const username,
	const char* const password,
	struct Credentials* const credentials
) {
	
	const int code = estrategia_authorize(username, password, credentials);
	return code;
	
}

int estrategia_medicina_get_resources(
	const struct Credentials* const credentials,
	struct Resources* const resources
) {
	
	estrategia_set_vertical(ESTRATEGIA_VERTICAL);
	
	const int code = estrategia_get_resources(credentials, resources);
	return code;
	
}

int estrategia_medicina_get_modules(
	const struct Credentials* const credentials,
	struct Resource* const resource
) {
	
	const int code = estrategia_get_modules(credentials, resource);
	return code;
	
}

int estrategia_medicina_get_module(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Module* const module
) {
	
	const int code = estrategia_get_module(credentials, resource, module);
	return code;
	
}

int estrategia_medicina_get_page(
	const struct Credentials* const credentials,
	const struct Resource* const resource,
	struct Page* const page
) {
	
	const int code = estrategia_get_page(credentials, resource, page);
	return code;
	
}