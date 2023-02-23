#include "resources.h"

#ifndef SPARKLEC_DISABLE_HOTMART
	#include "hotmart.h"
#endif

#ifdef SPARKLEC_DISABLE_ESTRATEGIA
	#define SPARKLEC_DISABLE_ESTRATEGIA_CONCURSOS
	#define SPARKLEC_DISABLE_ESTRATEGIA_VESTIBULARES
#endif

#ifndef SPARKLEC_DISABLE_ESTRATEGIA_CONCURSOS
	#include "estrategia_concursos.h"
#endif

#ifndef SPARKLEC_DISABLE_ESTRATEGIA_VESTIBULARES
	#include "estrategia_vestibulares.h"
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
	const char* directory;
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
		},
		.directory = "Hotmart"
	},
#endif
#ifndef SPARKLEC_DISABLE_ESTRATEGIA_CONCURSOS
	{
		.label = "Estratégia Concursos",
		.url = "https://www.estrategiaconcursos.com.br",
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
#ifndef SPARKLEC_DISABLE_ESTRATEGIA_VESTIBULARES
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
		},
		.directory = "IA Expert Academy"
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
