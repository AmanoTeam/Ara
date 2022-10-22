#include <stdlib.h>
#include <string.h>

#include "query.h"
#include "errors.h"

static const char AND[] = "&";
static const char EQUAL[] = "=";

static int put_parameter(struct Query* obj, const struct Parameter parameter) {
	
	const size_t size = obj->size + sizeof(struct Parameter) * 1;
	struct Parameter* parameters = realloc(obj->parameters, size);
	
	if (parameters == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	obj->size = size;
	obj->parameters = parameters;
	
	obj->parameters[obj->position++] = parameter;
	
	if (obj->slength > 0) {
		obj->slength += strlen(AND);
	}
	
	if (parameter.key != NULL) {
		obj->slength += strlen(parameter.key);
	}
	
	obj->slength += strlen(EQUAL);
	
	if (parameter.value != NULL) {
		obj->slength += strlen(parameter.value);
	}
	
	return UERR_MEMORY_ALLOCATE_FAILURE;
	
}

int add_parameter(struct Query* obj, const char* key, const char* value) {
	
	struct Parameter parameter = {};
	
	const size_t key_size = strlen(key);
	
	if (key_size > 0) {
		parameter.key = malloc(key_size + 1);
		
		if (parameter.key == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(parameter.key, key);
	}
	
	const size_t value_size = strlen(value);
	
	if (value_size > 0) {
		parameter.value = malloc(value_size + 1);
		
		if (parameter.value == NULL) {
			return UERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(parameter.value, value);
	}
	
	return put_parameter(obj, parameter);
	
}

int query_stringify(const struct Query obj, char** dst) {
	
	char* buffer = malloc(obj.slength + 500);
	
	if (buffer == NULL) {
		return UERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	memset(buffer, '\0', 1);
	
	for (int index = 0; index < obj.position; index++) {
		const struct Parameter parameter = obj.parameters[index];
		
		const char* key = parameter.key;
		const char* value = parameter.value;
		
		if (*buffer != '\0') {
			strcat(buffer, AND);
		}
		
		if (parameter.key != NULL) {
			strcat(buffer, parameter.key);
		}
		
		strcat(buffer, EQUAL);
		
		if (parameter.value != NULL) {
			strcat(buffer, parameter.value);
		}
	}
	
	*dst = buffer;
	
	return UERR_SUCCESS;
	
}

void query_free(struct Query* obj) {
	
	for (size_t index = 0; index < obj->position; index++) {
		struct Parameter* parameter = &obj->parameters[index];
		
		if (parameter->key != NULL) {
			free(parameter->key);
			parameter->key = NULL;
		}
		
		if (parameter->value != NULL) {
			free(parameter->value);
		}
	}
	
	obj->size = 0;
	obj->position = 0;
	
	free(obj->parameters);
	obj->parameters = NULL;
	
}
