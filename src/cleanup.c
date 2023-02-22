#include <stdlib.h>

#include "cleanup.h"

void _free_(char** ptr) {
	free(*ptr);
}
