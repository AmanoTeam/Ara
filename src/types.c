#include <stdlib.h>

#include "types.h"

void string_free(struct String* obj) {
	
	free(obj->s);
	obj->s = NULL;
	obj->slength = 0;
	
}

void credentials_free(struct Credentials* obj) {
	
	free(obj->access_token);
	obj->access_token = NULL;
	
	free(obj->refresh_token);
	obj->refresh_token = NULL;
	
	obj->expires_in = 0;
	
}
