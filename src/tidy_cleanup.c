#include "ttidy.h"
#include "tidy_cleanup.h"

void _tidy_release_(const tidy_doc_t* const* ptr) {
	tidy_release(*ptr);
}
