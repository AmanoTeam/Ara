#include <stdlib.h>

#include "cleanup.h"

void __free(char** ptr) {
	free(*ptr);
}
