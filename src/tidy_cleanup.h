#include "ttidy.h"

void _tidy_release_(const tidy_doc_t* const* ptr);

#define __tidy_release__ __attribute__((__cleanup__(_tidy_release_)))

#pragma once
