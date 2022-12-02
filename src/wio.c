#include <stdio.h>
#include <stdarg.h>

#include <windows.h>

#include "wio.h"

int __printf(const char* const format, ...) {
	
	va_list list;
	va_start(list, format);
	
	int size = vsnprintf(NULL, 0, format, list);
	
	if (size < 0) {
		return size;
	}
	
	char value[size + 1];
	size = vsnprintf(value, sizeof(value), format, list);
	
	if (size < 0) {
		return size;
	}
	
	va_end(list);
	
	int wcsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
	wchar_t wvalue[wcsize];
	MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wcsize);
	
	return wprintf(L"%ls", wvalue);
	
}

int __fprintf(FILE* const stream, const char* const format, ...) {
	
	va_list list;
	va_start(list, format);
	
	int size = vsnprintf(NULL, 0, format, list);
	
	if (size < 0) {
		return size;
	}
	
	char value[size + 1];
	size = vsnprintf(value, sizeof(value), format, list);
	
	if (size < 0) {
		return size;
	}
	
	va_end(list);
	
	int wcsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
	wchar_t wvalue[wcsize];
	MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, (int) (sizeof(wvalue) / sizeof(*wvalue)));
	
	return fwprintf(stream, L"%ls", wvalue);
	
}

char* __fgets(char* const s, const int n, FILE* const stream) {
	
	wchar_t ws[n];
	
	if (fgetws(ws, (int) (sizeof(ws) / sizeof(*ws)), stream) == NULL) {
		return NULL;
	}
	
	WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, (int) (sizeof(ws) / sizeof(*ws)), NULL, NULL);
	
	return s;
	
}
