int directory_exists(const char* const directory);
int create_directory(const char* const directory);
char to_hex(const char ch);
char from_hex(const char ch);
int isnumeric(const char* const s);
void normalize_filename(char* filename);
int expand_filename(const char* filename, char** fullpath);