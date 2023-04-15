#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int __printf(const char* const format, ...);
int __fprintf(FILE* const stream, const char* const format, ...);
char* __fgets(char* const s, const int n, FILE* const stream);

#ifdef __cplusplus
}
#endif

#define printf __printf
#define fprintf __fprintf
#define fgets __fgets

#pragma once
