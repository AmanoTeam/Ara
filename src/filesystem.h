char* get_current_directory(void);
char* get_app_filename(char* const filename);
int remove_file(const char* const filename);
int directory_exists(const char* const directory);
int file_exists(const char* const filename);
int create_directory(const char* const directory);
int move_file(const char* const source, const char* const destination);
long long get_file_size(const char* const filename);
int copy_file(const char* const source, const char* const destination);