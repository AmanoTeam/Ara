#include <stdlib.h>

#include "types.h"

void string_free(struct String* obj) {
	
	free(obj->s);
	obj->s = NULL;
	obj->slength = 0;
	
}