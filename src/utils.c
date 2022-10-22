#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
	#ifdef UNICODE
		#undef UNICODE
	#endif
	
	#ifdef _UNICODE
		#undef _UNICODE
	#endif
	
	#include <windows.h>
	#include <fileapi.h>
#else
	#include <unistd.h>
	#include <sys/stat.h>
	#include <errno.h>
	#include <glob.h>
#endif

static const char SLASH = '/';
static const char BACKSLASH = '\\';
static const char COLON = ':';

static const char INVALID_FILENAME_CHARS[] = {
	' ', '/', '\\', ':', '*', '?', '\"', '<', '>', '|', '^', '\x00'
};

void normalize_filename(char* filename) {
	
	char* ptr = strpbrk(filename, INVALID_FILENAME_CHARS);
	
	while (ptr != NULL) {
		*ptr = '_';
		ptr = strpbrk(ptr, INVALID_FILENAME_CHARS);
	}
	
}

char from_hex(const char ch) {
	
	if (ch <= '9' && ch >= '0') {
		return ch - '0';
	}
	
	 if (ch <= 'f' && ch >= 'a') {
		return ch - ('a' - 10);
	}
	
	if (ch <= 'F' && ch >= 'A') {
		return ch - ('A' - 10);
	}
	
	return '\0';
	
}

char to_hex(const char ch) {
	return ch + (ch > 9 ? ('a' - 10) : '0');
}

int isnumeric(const char* const s) {
	/*
	Return true (1) if the string is a numeric string, false (0) otherwise.
	
	A string is numeric if all characters in the string are numeric and there is at least one character in the string.
	*/
	
	for (size_t index = 0; index < strlen(s); index++) {
		const char ch = s[index];
		
		if (!isdigit(ch)) {
			return 0;
		}
	}
	
	return 1;
	
}

int expand_filename(const char* filename, char** fullpath) {
	
	#ifdef _WIN32
		char path[MAX_PATH];
		const DWORD code = GetFullPathNameA(filename, sizeof(path), path, NULL);
		
		if (code == 0) {
			return code;
		}
		
		*fullpath = malloc(code + 1);
		
		if (*fullpath == NULL) {
			return 0;
		}
		
		strcpy(*fullpath, path);
	#else
		*fullpath = realpath(filename, NULL);
		
		if (*fullpath == NULL) {
			return 0;
		}
	#endif
	
	return 1;
	
}


int directory_exists(const char* const directory) {
	
	#ifdef _WIN32
		const DWORD value = GetFileAttributesA(directory);
		return (value != -1 && ((value & FILE_ATTRIBUTE_DIRECTORY) > 0));
	#else
		struct stat st = {0};
		return (stat(directory, &st) >= 0 && S_ISDIR(st.st_mode));
	#endif
	
}

int is_absolute(const char* const path) {
	
	#ifdef _WIN32
		return ((*path == SLASH  || *path == BACKSLASH) || (strlen(path) > 1 && isalpha(*path) && path[1] == COLON));
	#else
		return (*path == SLASH);
	#endif
	
}

static int raw_create_dir(const char* const directory) {
	
	#ifdef _WIN32
		return (CreateDirectoryA(directory, NULL) == 1 || GetLastError() == ERROR_ALREADY_EXISTS);
	#else
		return (mkdir(directory, 0777) == 0 || errno == EEXIST);
	#endif
	
}

static int is_separator(const char c) {
	return (c == SLASH || c == BACKSLASH);
}


int create_directory(const char* const directory) {
	
	int omit_next = 0;
	
	#ifdef _WIN32
		omit_next = is_absolute(directory);
	#endif
	
	const char* start = directory;
	
	for (size_t index = 1; index < strlen(directory) + 1; index++) {
		const char* const ch = &directory[index];
		
		if (!(is_separator(*ch) || (*ch == '\0' && index > 1 && !is_separator(*(ch - 1))))) {
			continue;
		}
		
		if (omit_next) {
			omit_next = 0;
		} else {
			const size_t size = ch - start;
			
			char directory[size + 1];
			memcpy(directory, start, size);
			directory[size] = '\0';
			
			if (!raw_create_dir(directory)) {
				return 0;
			}
		}
	}
	
	return 1;
	
}

int iter(const char* const directory) {
	
	
	#ifdef _WIN32
		WIN32_FIND_DATA data = {};
		const HANDLE handle = FindFirstFileA(directory, &data);
		
		if (handle == INVALID_HANDLE_VALUE) {
			return 0;
		}
		
		while (1) {
			puts(data.cFileName);
			
			if (FindNextFileA(handle, &data) == 0) {
				FindClose(handle);
				
				if (GetLastError() == ERROR_NO_MORE_FILES) {
					break;
				}
				
				return 0;
			}
		}
		
		FindClose(handle);
		
	#else
		glob_t glb = {0};
		
		if (glob(directory, 0, NULL, &glb) != 0) {
			return 0;
		}
		
		for (size_t index = 0; index < glb.gl_pathc; index++) {
			const char* const filename = glb.gl_pathv[index];
			puts(filename);
		}
		
		globfree(&glb);
	#endif
	
	return 0;
}
/*
int main() {
	
	#ifdef _WIN32
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
	#endif
	printf(("äAAàA"));
	printf("\r\n");
	printf("directory_exists -> %i\n", directory_exists("b"));
	create_directory("/home/snow/b/b/b");
	iter("./*");
}

int main(void) {
	char* a;
	expand_filename(".", &a);
	puts(a);
}
*/
