#include <stdlib.h>

struct String {
	char *s;
	size_t slength;
};

void string_free(struct String* obj);

#pragma once