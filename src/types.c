#include <stdlib.h>

#include "types.h"

void string_array_free(string_array_t* obj) {
	
	for (size_t index = 0; index < obj->offset; index++) {
		char* item = obj->items[index];
		free(item);
	}
	
	if (obj->items != NULL) {
		free(obj->items);
		obj->items = NULL;
	}
	
	obj->offset = 0;
	obj->size = 0;
	
}

void jint_array_free(jint_array_t* obj) {
	
	if (obj->items != NULL) {
		free(obj->items);
		obj->items = NULL;
	}
	
	obj->offset = 0;
	obj->size = 0;
	
}
