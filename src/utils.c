#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
	#include <windows.h>
	#include <fileapi.h>
	#include <direct.h>
	
	#define NAME_MAX (_MAX_FNAME - 1)
#else
	#include <unistd.h>
	#include <sys/stat.h>
	#include <errno.h>
	#include <limits.h>
	
	#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
		#include <sys/sysctl.h>
	#endif
	
	#ifdef __linux__
		#ifndef PATH_MAX
			#define PATH_MAX 4096
		#endif
	#endif
	
	#define FILERW_MAX_CHUNK_SIZE 8192
#endif

#include "utils.h"
#include "symbols.h"
#include "fstream.h"

static const char INVALID_FILENAME_CHARS[] = {
	'\'', '%', '"', ' ', '/', '\\', ':', '*', '?', '\"', '<', '>', '|', '^', '\x00'
};

char* basename(const char* const path) {
	/*
	Returns the final component of a pathname.
	*/
	
	char* last_comp = (char*) path;
	
	while (1) {
		char* slash_at = strchr(last_comp, *PATH_SEPARATOR);
		
		if (slash_at == NULL) {
			break;
		}
		
		last_comp = slash_at + 1;
	}
	
	return last_comp;
	
}

char* get_file_extension(const char* const filename) {
	
	if (*filename == '\0') {
		return NULL;
	}
	
	const char* const last_part = basename(filename);
	
	char* start = strstr(last_part, DOT);
	
	if (start == NULL) {
		return NULL;
	}
	
	while (1) {
		char* const tmp = strstr(start + 1, DOT);
		
		if (tmp == NULL) {
			break;
		}
		
		start = tmp;
	}
	
	if (start == filename) {
		return NULL;
	}
	
	start++;
	
	return start;
	
}

char* get_current_directory(void) {
	
	#ifdef _WIN32
		#ifdef _UNICODE
			wchar_t wpwd[PATH_MAX];
			const DWORD wpwds = (DWORD) (sizeof(wpwd) / sizeof(*wpwd));
			
			const DWORD code = GetCurrentDirectoryW(wpwds, wpwd);
			
			if (code == 0 || code > wpwds) {
				return 0;
			}
			
			const int size = WideCharToMultiByte(CP_UTF8, 0, wpwd, -1, NULL, 0, NULL, NULL);
			
			char* pwd = malloc((size_t) size);
			
			if (pwd == NULL) {
				return NULL;
			}
			
			WideCharToMultiByte(CP_UTF8, 0, wpwd, -1, pwd, size, NULL, NULL);
		#else
			char cpwd[PATH_MAX];
			const DWORD cpwds = (DWORD) sizeof(cpwd);
			
			const DWORD code = GetCurrentDirectoryA(cpwds, cpwd);
			
			if (code == 0 || code > cpwds) {
				return 0;
			}
			
			char* pwd = malloc(strlen(cpwd) + 1);
			
			if (pwd == NULL) {
				return NULL;
			}
			
			strcpy(pwd, cpwd);
		#endif
	#else
		char cpwd[PATH_MAX];
		
		if (getcwd(cpwd, sizeof(cpwd) / sizeof(*cpwd)) == NULL) {
			return NULL;
		}
		
		char* pwd = malloc(strlen(cpwd) + 1);
		
		if (pwd == NULL) {
			return NULL;
		}
		
		strcpy(pwd, cpwd);
	#endif
	
	return pwd;
	
}

int execute_shell_command(const char* const command) {
	
	#if defined(_WIN32) && defined(_UNICODE)
		const int wcsize = MultiByteToWideChar(CP_UTF8, 0, command, -1, NULL, 0);
		wchar_t wcommand[wcsize];
		MultiByteToWideChar(CP_UTF8, 0, command, -1, wcommand, wcsize);
		
		const int code = _wsystem(wcommand);
	#else
		const int code = system(command);
	#endif
	
	int exit_code = 0;
	
	#ifdef __linux__
		if (WIFSIGNALED(code)) {
			exit_code = 128 + WTERMSIG(code);
		} else {
			exit_code = WEXITSTATUS(code);
		}
	#else
		exit_code = code;
	#endif
	
	return exit_code;
	
}

int is_administrator(void) {

	#ifdef _WIN32
		SID_IDENTIFIER_AUTHORITY authority = {
			.Value = SECURITY_NT_AUTHORITY
		};
		
		PSID group = {0};
		
		BOOL status = AllocateAndInitializeSid(
			&authority,
			2,
			SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS,
			0,
			0,
			0,
			0,
			0,
			0,
			&group
		);
		
		if (!status) {
			return 0;
		}
		
		BOOL is_member = 0;
		status = CheckTokenMembership(0, group, &is_member);
		
		FreeSid(group);
		
		if (!status) {
			return 0;
		}
		
		return (int) is_member;
	#else
		return geteuid() == 0;
	#endif
	
}

char* get_configuration_directory(void) {
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const wchar_t* const wdirectory = _wgetenv(L"APPDATA");
			
			if (wdirectory == NULL) {
				return NULL;
			}
			
			const int size = WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, NULL, 0, NULL, NULL);
			
			char directory[(size_t) size];
			
			WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, directory, size, NULL, NULL);
		#else
			const char* const directory = getenv("APPDATA");
			
			if (directory == NULL) {
				return NULL;
			}
		#endif
	#else
		const char* const directory = getenv("XDG_CONFIG_HOME");
		
		if (directory == NULL) {
			const char* const config_directory = ".config";
			const char* const home_directory = getenv("HOME");
			
			char* configuration_directory = malloc(strlen(home_directory) + strlen(PATH_SEPARATOR) + strlen(config_directory) + strlen(PATH_SEPARATOR) + 1);
			
			if (configuration_directory == NULL) {
				return NULL;
			}
			
			strcpy(configuration_directory, home_directory);
			strcat(configuration_directory, PATH_SEPARATOR);
			strcat(configuration_directory, config_directory);
			strcat(configuration_directory, PATH_SEPARATOR);
			
			return configuration_directory;
		}
	#endif
	
	const int trailing_separator = strlen(directory) > 0 && *(strchr(directory, '\0') - 1) == *PATH_SEPARATOR;
	char* configuration_directory = malloc(strlen(directory) + (trailing_separator ? 0 : strlen(PATH_SEPARATOR)) + 1);
	
	if (configuration_directory == NULL) {
		return NULL;
	}
	
	strcpy(configuration_directory, directory);
	
	if (!trailing_separator) {
		strcat(configuration_directory, PATH_SEPARATOR);
	}
	
	return configuration_directory;
	
}

char* get_temporary_directory(void) {
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const size_t wsize = (size_t) GetTempPathW(0, NULL);
			
			if (wsize == 0) {
				return NULL;
			}
			
			wchar_t wdirectory[wsize + 1];
			
			const DWORD code = GetTempPathW((DWORD) (sizeof(wdirectory) / sizeof(*wdirectory)), wdirectory);
			
			if (code == 0) {
				return 0;
			}
			
			const int csize = WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, NULL, 0, NULL, NULL);
			char* temporary_directory = malloc((size_t) csize);
			
			if (temporary_directory == NULL) {
				return NULL;
			}
			
			WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, temporary_directory, csize, NULL, NULL);
		#else
			const size_t size = (size_t) GetTempPathA(0, NULL);
			
			if (size == 0) {
				return NULL;
			}
			
			char directory[size + 1];
			const DWORD code = GetTempPathA((DWORD) sizeof(directory), directory);
			
			if (code == 0) {
				return 0;
			}
			
			char* temporary_directory = malloc(sizeof(directory));
			
			if (temporary_directory == NULL) {
				return NULL;
			}
			
			strcpy(temporary_directory, directory);
		#endif
	#else
		const char* const keys[] = {
			"TMPDIR",
			"TEMP",
			"TMP",
			"TEMPDIR"
		};
		
		for (size_t index = 0; index < (sizeof(keys) / sizeof(*keys)); index++) {
			const char* const key = keys[index];
			const char* const value = getenv(key);
			
			if (key == NULL || !directory_exists(value)) {
				continue;
			}
			
			char* temporary_directory = malloc(strlen(value) + 1);
			
			if (temporary_directory == NULL) {
				return NULL;
			}
			
			strcpy(temporary_directory, value);
			
			return temporary_directory;
		}
		
		const char* const tmp = "/tmp";
		
		if (!directory_exists(tmp)) {
			return NULL;
		}
		
		char* temporary_directory = malloc(strlen(tmp) + 1);
		
		if (temporary_directory == NULL) {
			return NULL;
		}
		
		strcpy(temporary_directory, tmp);
	#endif
	
	if (strlen(temporary_directory) > 1) {
		char* ptr = strchr(temporary_directory, '\0') - 1;
		
		if (*ptr == *PATH_SEPARATOR) {
			*ptr = '\0';
		}
	}
	
	return temporary_directory;
	
}

char* normalize_filename(char* filename) {
	
	for (size_t index = 0; index < strlen(filename); index++) {
		char* ch = &filename[index];
		
		if (iscntrl(*ch) || strchr(INVALID_FILENAME_CHARS, *ch) != NULL) {
			*ch = '_';
		}
	}
	
	return filename;
	
}

char* normalize_directory(char* directory) {
	
	normalize_filename(directory);
	
	if (strlen(directory) > NAME_MAX) {
		directory[NAME_MAX - 1] = '\0';
	}
	
	return directory;
	
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

size_t intlen(const int value) {
	
	int val = value;
	size_t size = 0;
	
	do {
		val /= 10;
		size++;
	} while (val > 0);
	
	return size;
	
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

int remove_file(const char* const filename) {
	/*
	Removes the file. On Windows, ignores the read-only attribute.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			wchar_t wfilename[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wfilename, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename + wcslen(WIN10LP_PREFIX), wcsize);
			
			return DeleteFileW(wfilename) == 1;
		#else
			return DeleteFileA(filename) == 1;
		#endif
	#else
		return unlink(filename) == 0;
	#endif
	
}

int directory_exists(const char* const directory) {
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			wchar_t wdirectory[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wdirectory, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, directory, -1, wdirectory + wcslen(WIN10LP_PREFIX), wcsize);
			
			const DWORD value = GetFileAttributesW(wdirectory);
		#else
			const DWORD value = GetFileAttributesA(directory);
		#endif
		
		return (value != INVALID_FILE_ATTRIBUTES && ((value & FILE_ATTRIBUTE_DIRECTORY) > 0));
	#else
		struct stat st = {0};
		return (stat(directory, &st) >= 0 && S_ISDIR(st.st_mode));
	#endif
	
}

int file_exists(const char* const filename) {
	
	/*
	Returns 1 (true) if file exists and is a regular file or symlink, 0 (false) otherwise.
	Directories, device files, named pipes and sockets return false.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			wchar_t wfilename[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wfilename, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename + wcslen(WIN10LP_PREFIX), wcsize);
			
			const DWORD value = GetFileAttributesW(wfilename);
		#else
			const DWORD value = GetFileAttributesA(filename);
		#endif
		
		return (value != INVALID_FILE_ATTRIBUTES && ((value & FILE_ATTRIBUTE_DIRECTORY) == 0));
	#else
		struct stat st = {0};
		return (stat(filename, &st) == 0 && S_ISREG(st.st_mode));
	#endif
	
}

static int raw_create_dir(const char* const directory) {
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			wchar_t wdirectory[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wdirectory, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, directory, -1, wdirectory + wcslen(WIN10LP_PREFIX), wcsize);
			
			const BOOL code = CreateDirectoryW(wdirectory, NULL);
		#else
			const BOOL code = CreateDirectoryA(directory, NULL);
		#endif
			
		return (code == 1 || GetLastError() == ERROR_ALREADY_EXISTS);
	#else
		return (mkdir(directory, 0777) == 0 || errno == EEXIST);
	#endif
	
}

int is_absolute(const char* const path) {
	
	#ifdef _WIN32
		return (*path == *PATH_SEPARATOR || (strlen(path) > 1 && isalpha(*path) && path[1] == *COLON));
	#else
		return (*path == *PATH_SEPARATOR);
	#endif
	
}

int create_directory(const char* const directory) {
	
	int omit_next = 0;
	
	#ifdef _WIN32
		omit_next = is_absolute(directory);
	#endif
	
	const char* start = directory;
	
	for (size_t index = 1; index < strlen(directory) + 1; index++) {
		const char* const ch = &directory[index];
		
		if (!(*ch == *PATH_SEPARATOR || (*ch == '\0' && index > 1 && *(ch - 1) != *PATH_SEPARATOR))) {
			continue;
		}
		
		if (omit_next) {
			omit_next = 0;
		} else {
			const size_t size = (size_t) (ch - start);
			
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

int copy_file(const char* const source, const char* const destination) {
	/*
	Copies a file from source to destination.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			int wcsize = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
			wchar_t wsource[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wsource, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, source, -1, wsource + wcslen(WIN10LP_PREFIX), wcsize);
			
			wcsize = MultiByteToWideChar(CP_UTF8, 0, destination, -1, NULL, 0);
			wchar_t wdestination[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wdestination, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, destination, -1, wdestination + wcslen(WIN10LP_PREFIX), wcsize);
			
			return CopyFileW(wsource, wdestination, FALSE) == 1;
		#else
			return CopyFileA(source, destination, FALSE) == 1;
		#endif
	#else
		// Generic version which works for any platform
		struct FStream* const input_stream = fstream_open(source, "r");
		
		if (input_stream == NULL) {
			return 0;
		}
		
		struct FStream* const output_stream = fstream_open(destination, "wb");
		
		if (output_stream == NULL) {
			fstream_close(input_stream);
			return 0;
		}
		
		while (1) {
			char chunk[FILERW_MAX_CHUNK_SIZE];
			const ssize_t size = fstream_read(input_stream, chunk, sizeof(chunk));
			
			if (size == -1) {
				fstream_close(input_stream);
				fstream_close(output_stream);
				return 0;
			}
			
			if (size == 0) {
				fstream_close(input_stream);
				fstream_close(output_stream);
				break;
			}
			
			const int status = fstream_write(output_stream, chunk, (size_t) size);
			
			if (!status) {
				fstream_close(input_stream);
				fstream_close(output_stream);
				return 0;
			}
		}
		
		return 1;
	#endif

}

int move_file(const char* const source, const char* const destination) {
	/*
	Moves a file from source to destination. Symlinks are not followed: if source is a symlink, it is itself moved, not it's target.
	If destination already exists, it will be overwritten.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			int wcsize = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
			wchar_t wsource[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wsource, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, source, -1, wsource + wcslen(WIN10LP_PREFIX), wcsize);
			
			wcsize = MultiByteToWideChar(CP_UTF8, 0, destination, -1, NULL, 0);
			wchar_t wdestination[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wdestination, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, destination, -1, wdestination + wcslen(WIN10LP_PREFIX), wcsize);
			
			return MoveFileExW(wsource, wdestination, MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING) == 1;
		#else
			return MoveFileExA(source, destination, MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING) == 1;
		#endif
	#else
		const int code = rename(source, destination);
		
		if (code == 0) {
			return 1;
		}
		
		if (errno == EXDEV) {
			const int code = copy_file(source, destination);
			
			if (code) {
				remove_file(source);
			}
			
			return code;
		}
		
		return code;
	#endif
	
}

int get_app_filename(char* const filename, const size_t size) {
	
	#ifdef _WIN32
		#ifdef _UNICODE
			wchar_t wfilename[size];
			const DWORD code = GetModuleFileNameW(0, wfilename, (DWORD) (sizeof(wfilename) / sizeof(*wfilename)));
			
			if (code == 0) {
				return 0;
			}
			
			const int msize = WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, filename, msize, NULL, NULL);
		#else
			const DWORD code = GetModuleFileNameA(0, filename, (DWORD) size);
			
			if (code == 0) {
				return 0;
			}
		#endif
	#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
		#ifdef __NetBSD__
			const int call[] = {
				CTL_KERN,
				KERN_PROC_ARGS,
				-1,
				KERN_PROC_PATHNAME
			};
		#else
			const int call[] = {
				CTL_KERN,
				KERN_PROC,
				KERN_PROC_PATHNAME,
				-1
			};
		#endif
		
		size_t nsize = size;
		const int code = sysctl(call, sizeof(call) / sizeof(*call), filename, &nsize, NULL, 0);
		
		if (code != 0) {
			return 0;
		}
	#else
		if (readlink("/proc/self/exe", filename, size) == -1) {
			return 0;
		}
	#endif
	
	return 1;
	
}

char* get_parent_directory(const char* const source, char* const destination, const size_t depth) {
	
	const size_t len = strlen(source);
	size_t current_depth = 1;
	
	for (ssize_t index = (ssize_t) (len - 1); index >= 0; index--) {
		const char* const ch = &source[index];
		
		if (*ch == *PATH_SEPARATOR && current_depth++ == depth) {
			const size_t size = (size_t) (ch - source);
			
			if (size > 0) {
				memcpy(destination, source, size);
				destination[size] = '\0';
			} else {
				strcpy(destination, PATH_SEPARATOR);
			}
			
			break;
		}
		
		if (index == 0 && len > 2 && isalpha(*source) && source[1] == *COLON && source[2] == *PATH_SEPARATOR) {
			memcpy(destination, source, 3);
			destination[3] = '\0';
		}
	}
	
	return destination;
	
}

long long get_file_size(const char* const filename) {
	
	#ifdef _WIN32
		WIN32_FIND_DATA data = {0};
		
		#ifdef _UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			wchar_t wfilename[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(wfilename, WIN10LP_PREFIX);
			MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename + wcslen(WIN10LP_PREFIX), wcsize);
			
			const HANDLE handle = FindFirstFileW(wfilename, &data);
		#else
			const HANDLE handle = FindFirstFileA(filename, &data);
		#endif
		
		if (handle == INVALID_HANDLE_VALUE) {
			return 0;
		}
		
		FindClose(handle);
		
		return (data.nFileSizeHigh * MAXDWORD) + data.nFileSizeLow;
	#else
		struct stat st = {0};
		return (stat(filename, &st) >= 0 ? st.st_size : 0);
	#endif
	
}

int hashs(const char* const s) {
	
	int value = 0;
	
	for (size_t index = 0; index < strlen(s); index++) {
		const int ch = s[index];
		value += ch + (int) index;
	}
	
	return value;
	
}