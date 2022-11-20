#include <stdio.h>

int __printf(const char* const format, ...);
int __fprintf(FILE* const stream, const char* const format, ...);
char* __fgets(char* const s, const int n, FILE* const stream);

#define printf __printf
#define fprintf __fprintf
#define fgets __fgets

#pragma once