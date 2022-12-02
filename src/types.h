#include <stdlib.h>
#include <stdio.h>

#include <curl/curl.h>

#include "fstream.h"

typedef struct string_array_t {
	size_t offset;
	size_t size;
	char** items;
} string_array_t;

struct String {
	char *s;
	size_t slength;
};

struct Download {
	CURL* handle;
	char* filename;
	struct FStream* stream;
};

void string_free(struct String* obj);
void string_array_free(string_array_t* obj);

#pragma once
