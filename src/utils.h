#include <stdlib.h>

#ifdef _WIN32
	#include <windows.h>
	#include <fileapi.h>
	
	#ifdef UNICODE
		struct WalkDir {
			size_t index;
			char* last_path;
			HANDLE handle;
			WIN32_FIND_DATAW data;
		};
	#else
		struct WalkDir {
			size_t index;
			char* last_path;
			HANDLE handle;
			WIN32_FIND_DATAA data;
		};
	#endif
#else
	#include <glob.h>
	
	struct WalkDir {
		size_t index;
		char* last_path;
		glob_t data;
	};
#endif

int directory_exists(const char* const directory);
int file_exists(const char* const filename);
int create_directory(const char* const directory);
int remove_file(const char* const filename);
char to_hex(const char ch);
char from_hex(const char ch);
size_t intlen(const int value);
int isnumeric(const char* const s);
void normalize_filename(char* filename);
char* get_current_directory(void);
int expand_filename(const char* filename, char** fullpath);
int execute_shell_command(const char* const command);
int is_administrator(void);
const char* get_file_extension(const char* const filename);
char* get_configuration_directory(void);