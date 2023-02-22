#include "buffer.h"

#define __buffer_free__ __attribute__((__cleanup__(buffer_free)))

#pragma once