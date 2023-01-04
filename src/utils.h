#include <stdlib.h>

int directory_exists(const char* const directory);
int file_exists(const char* const filename);
int create_directory(const char* const directory);
int remove_file(const char* const filename);
char to_hex(const char ch);
char from_hex(const char ch);
size_t intlen(const int value);
int isnumeric(const char* const s);
char* normalize_filename(char* filename);
char* get_current_directory(void);
char* basename(const char* const path);
int expand_filename(const char* filename, char** fullpath);
int execute_shell_command(const char* const command);
int is_administrator(void);
char* get_file_extension(const char* const filename);
char* get_configuration_directory(void);
char* get_temporary_directory(void);
int copy_file(const char* const source, const char* const destination);
int move_file(const char* const source, const char* const destination);
int is_absolute(const char* const path);
int get_app_filename(char* const filename, const size_t size);
char* get_parent_directory(const char* const source, char* const destination, const size_t depth);
long long get_file_size(const char* const filename);
int hashs(const char* const s);

#pragma once
