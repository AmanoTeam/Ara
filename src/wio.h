#include <stdio.h>

#include <windows.h>

int __printf(const char* const format, ...);
int __fprintf(FILE* const stream, const char* const format, ...);
FILE* __fopen(const char* const filename, const char* const mode);
char* __fgets(char* const s, const int n, FILE* const stream);

#define printf __printf
#define fprintf __fprintf
#define fopen __fopen
#define fgets __fgets

#pragma once