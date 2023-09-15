#include "resources.h"

#ifndef ARA_DISABLE_HOTMART
	#include "hotmart.h"
#endif

#ifdef ARA_DISABLE_ESTRATEGIA
	#define ARA_DISABLE_ESTRATEGIA_CONCURSOS
	#define ARA_DISABLE_ESTRATEGIA_VESTIBULARES
	#define ARA_DISABLE_ESTRATEGIA_MILITARES
#endif

#ifndef ARA_DISABLE_ESTRATEGIA_CONCURSOS
	#include "estrategia_concursos.h"
#endif

#ifndef ARA_DISABLE_ESTRATEGIA_VESTIBULARES
	#include "estrategia_vestibulares.h"
#endif

#ifndef ARA_DISABLE_ESTRATEGIA_MILITARES
	#include "estrategia_militares.h"
#endif

#ifndef ARA_DISABLE_IAEXPERT
	#include "iaexpert.h"
#endif

#ifndef ARA_DISABLE_KIWIFY
	#include "kiwify.h"
#endif

struct ProviderMethods {
	int (*authorize)(const char* const, const char* const, struct Credentials* const);
	int (*get_resources)(const struct Credentials* const, struct Resources* const);
	int (*get_modules)(const struct Credentials* const, struct Resource* const);
	int (*get_module)(const struct Credentials* const, const struct Resource* const, struct Module*);
	int (*get_page)(const struct Credentials* const, const struct Resource* const, struct Page* const);
};

struct Provider {
	const char* label;
	const char* url;
	struct ProviderMethods methods;
	const char* directory;
};

static const struct Provider PROVIDERS[] = {
#ifndef ARA_DISABLE_HOTMART
	{
		.label = "Hotmart",
		.url = "https://hotmart.com",
		.methods = {
			.authorize = &hotmart_authorize,
			.get_resources = &hotmart_get_resources,
			.get_modules = &hotmart_get_modules,
			.get_module = &hotmart_get_module,
			.get_page = &hotmart_get_page
		},
		.directory = "Hotmart"
	},
#endif
#ifndef ARA_DISABLE_ESTRATEGIA_CONCURSOS
	{
		.label = "Estratégia Concursos",
		.url = "https://estrategiaconcursos.com.br",
		.methods = {
			.authorize = &estrategia_concursos_authorize,
			.get_resources = &estrategia_concursos_get_resources,
			.get_modules = &estrategia_concursos_get_modules,
			.get_module = &estrategia_concursos_get_module,
			.get_page = &estrategia_concursos_get_page
		},
		.directory = "Estratégia"
	},
#endif
#ifndef ARA_DISABLE_ESTRATEGIA_VESTIBULARES
	{
		.label = "Estratégia Vestibulares",
		.url = "https://vestibulares.estrategia.com",
		.methods = {
			.authorize = &estrategia_vestibulares_authorize,
			.get_resources = &estrategia_vestibulares_get_resources,
			.get_modules = &estrategia_vestibulares_get_modules,
			.get_module = &estrategia_vestibulares_get_module,
			.get_page = &estrategia_vestibulares_get_page
		},
		.directory = "Estratégia"
	},
#endif
#ifndef ARA_DISABLE_ESTRATEGIA_MILITARES
	{
		.label = "Estratégia Militares",
		.url = "https://militares.estrategia.com",
		.methods = {
			.authorize = &estrategia_militares_authorize,
			.get_resources = &estrategia_militares_get_resources,
			.get_modules = &estrategia_militares_get_modules,
			.get_module = &estrategia_militares_get_module,
			.get_page = &estrategia_militares_get_page
		},
		.directory = "Estratégia"
	},
#endif
#ifndef ARA_DISABLE_IAEXPERT
	{
		.label = "IA Expert Academy",
		.url = "https://iaexpert.academy",
		.methods = {
			.authorize = &iaexpert_authorize,
			.get_resources = &iaexpert_get_resources,
			.get_modules = &iaexpert_get_modules,
			.get_module = &iaexpert_get_module,
			.get_page = &iaexpert_get_page
		},
		.directory = "IAExpertAcademy"
	},
#endif
#ifndef ARA_DISABLE_KIWIFY
	{
		.label = "Kiwify",
		.url = "https://kiwify.com.br",
		.methods = {
			.authorize = &kiwify_authorize,
			.get_resources = &kiwify_get_resources,
			.get_modules = &kiwify_get_modules,
			.get_module = &kiwify_get_module,
			.get_page = &kiwify_get_page
		},
		.directory = "Kiwify"
	},
#endif
};

#define PROVIDERS_NUM (sizeof(PROVIDERS) / sizeof(*PROVIDERS))

#pragma once
