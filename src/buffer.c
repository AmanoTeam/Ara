#include <stdlib.h>

#include "buffer.h"

void buffer_free(buffer_t* obj) {
	
	free(obj->s);
	obj->s = NULL;
	obj->slength = 0;
	
}