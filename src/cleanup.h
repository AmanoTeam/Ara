void _free_(char** ptr);

#define __free__ __attribute__((__cleanup__(_free_)))

#pragma once
