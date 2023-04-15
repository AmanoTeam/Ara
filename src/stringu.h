#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

char* extract_filename(const char* const path);
char* get_file_extension(const char* const filename);
char* normalize_filename(char* filename);
char* normalize_directory(char* directory);
char from_hex(const char ch);
size_t intlen(const int value);
int isnumeric(const char* const s);
char* get_parent_directory(const char* const source, char* const destination, const size_t depth);
int hashs(const char* const s);
char* remove_file_extension(char* const filename);
char* strip(char* const s);

#ifdef __cplusplus
}
#endif

#pragma once
