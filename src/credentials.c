#include <stdlib.h>

#include "credentials.h"

void credentials_free(struct Credentials* obj) {
	
	free(obj->access_token);
	obj->access_token = NULL;
	
}
