struct Parameter {
	char* key;
	char* value;
};

struct Query {
	size_t size;
	size_t position;
	size_t slength;
	struct Parameter* parameters;
};

void query_free(struct Query* obj);
int query_add_parameter(struct Query* obj, const char* key, const char* value);
int query_stringify(const struct Query obj, char** dst);

#define __query_free__ __attribute__((__cleanup__(query_free)))

#pragma once
