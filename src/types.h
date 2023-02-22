#include <stdlib.h>
#include <stdio.h>

#include <curl/curl.h>
#include <jansson.h>

#include "fstream.h"

typedef struct string_array_t {
	size_t offset;
	size_t size;
	char** items;
} string_array_t;

typedef struct jint_array_t {
	size_t offset;
	size_t size;
	json_int_t* items;
} jint_array_t;

struct Download {
	CURL* handle;
	char* filename;
	struct FStream* stream;
};

void string_array_free(string_array_t* obj);
void jint_array_free(jint_array_t* obj);

#pragma once
