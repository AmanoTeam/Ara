void __free(char** ptr);

#define __free__ __attribute__((__cleanup__(__free)))

#pragma once
