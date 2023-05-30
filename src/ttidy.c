#include "ttidy.h"

void __tidy_release(const tidy_doc_t* const* ptr) {
	tidy_release(*ptr);
}
