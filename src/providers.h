#include "resources.h"

#ifndef SPARKLEC_DISABLE_HOTMART
	#include "hotmart.h"
#endif

#ifndef SPARKLEC_DISABLE_ESTRATEGIA
	#include "estrategia.h"
#endif

#ifndef SPARKLEC_DISABLE_CYBERCLASS
	#include "cyberclass.h"
#endif

#ifndef SPARKLEC_DISABLE_IAEXPERT
	#include "iaexpert.h"
#endif

#ifndef SPARKLEC_DISABLE_QCONCURSOS
	#include "qconcursos.h"
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
};

static const struct Provider PROVIDERS[] = {
#ifndef SPARKLEC_DISABLE_HOTMART
	{
		.label = "Hotmart",
		.url = "https://hotmart.com",
		.methods = {
			.authorize = &hotmart_authorize,
			.get_resources = &hotmart_get_resources,
			.get_modules = &hotmart_get_modules,
			.get_module = &hotmart_get_module,
			.get_page = &hotmart_get_page
		}
	},
#endif
#ifndef SPARKLEC_DISABLE_ESTRATEGIA
	{
		.label = "Estratégia Concursos",
		.url = "https://www.estrategiaconcursos.com.br",
		.methods = {
			.authorize = &estrategia_authorize,
			.get_resources = &estrategia_get_resources,
			.get_modules = &estrategia_get_modules,
			.get_module = &estrategia_get_module,
			.get_page = &estrategia_get_page
		}
	},
#endif
#ifndef SPARKLEC_DISABLE_CYBERCLASS
	{
		.label = "CyberClass",
		.url = "https://www.cyberclass.com.br",
		.methods = {
			.authorize = &cyberclass_authorize,
			.get_resources = &cyberclass_get_resources,
			.get_modules = &cyberclass_get_modules,
			.get_module = &cyberclass_get_module,
			.get_page = &cyberclass_get_page
		}
	},
#endif
#ifndef SPARKLEC_DISABLE_IAEXPERT
	{
		.label = "IA Expert Academy",
		.url = "https://iaexpert.academy",
		.methods = {
			.authorize = &iaexpert_authorize,
			.get_resources = &iaexpert_get_resources,
			.get_modules = &iaexpert_get_modules,
			.get_module = &iaexpert_get_module,
			.get_page = &iaexpert_get_page
		}
	},
#endif
#ifndef SPARKLEC_DISABLE_QCONCURSOS
	{
		.label = "QConcursos",
		.url = "https://www.qconcursos.com",
		.methods = {
			.authorize = &qconcursos_authorize,
			.get_resources = &qconcursos_get_resources,
			.get_modules = &qconcursos_get_modules,
			.get_module = &qconcursos_get_module,
			.get_page = &qconcursos_get_page
		}
	}
#endif
};

#define PROVIDERS_NUM (sizeof(PROVIDERS) / sizeof(*PROVIDERS))

/*
#if PROVIDERS_NUM < 1
	#error "PROVIDERS_NUM should be greater than 0"
#endif
*/
#pragma once
