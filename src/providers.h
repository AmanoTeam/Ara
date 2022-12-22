#include "resources.h"
#include "hotmart.h"
#include "estrategia.h"
#include "cyberclass.h"

struct ProviderMethods {
	int (*authorize)(const char* const, const char* const, struct Credentials* const);
	int (*get_resources)(const struct Credentials* const, struct Resources* const);
	int (*get_modules)(const struct Credentials* const, struct Resource* const);
	int (*get_page)(const struct Credentials* const, const struct Resource* const, struct Page* const);
};

struct Provider {
	const char* label;
	const char* url;
	struct ProviderMethods methods;
};

static const struct Provider PROVIDERS[] = {
	{
		.label = "Hotmart",
		.url = "https://hotmart.com/pt-br",
		.methods = {
			.authorize = &hotmart_authorize,
			.get_resources = &hotmart_get_resources,
			.get_modules = &hotmart_get_modules,
			.get_page = &hotmart_get_page
		}
	},
	{
		.label = "Estratégia Concursos",
		.url = "https://www.estrategiaconcursos.com.br",
		.methods = {
			.authorize = &estrategia_authorize,
			.get_resources = &estrategia_get_resources,
			.get_modules = &estrategia_get_modules,
			.get_page = &estrategia_get_page
		}
	},
	{
		.label = "CyberClass",
		.url = "https://www.cyberclass.com.br",
		.methods = {
			.authorize = &cyberclass_authorize,
			.get_resources = &cyberclass_get_resources,
			.get_modules = &cyberclass_get_modules,
			.get_page = &cyberclass_get_page
		}
	}
};

#pragma once
