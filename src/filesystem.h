char* get_current_directory(void);
char* get_app_filename(char* const filename);
int remove_file(const char* const filename);
int remove_recursive(const char* const directory, int remove_itself);
int directory_exists(const char* const directory);
int file_exists(const char* const filename);
int create_directory(const char* const directory);
int move_file(const char* const source, const char* const destination);
int copy_file(const char* const source, const char* const destination);
int directory_empty(const char* const directory);

#ifdef _WIN32
	int is_absolute(const char* const path);
#endif

#define remove_directory(directory) remove_recursive(directory, 1)
#define remove_directory_contents(directory) remove_recursive(directory, 0)
