#include <stdlib.h>

typedef struct buffer_t {
	char *s;
	size_t slength;
} buffer_t;

void buffer_free(buffer_t* obj);

#define __buffer_free__ __attribute__((__cleanup__(buffer_free)))

#pragma once
