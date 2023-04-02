#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#include "symbols.h"
#include "os.h"
#include "filesystem.h"

#ifndef HAIKU
	int is_administrator(void) {
		/*
		Returns whether the caller's process is a member of the Administrators local
		group (on Windows) or a root (on POSIX), via "geteuid() == 0".
		
		Returns (1) on true, (0) on false, (-1) on error.
		*/
		
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
			
			if (status == 0) {
				return -1;
			}
			
			BOOL is_member = 0;
			status = CheckTokenMembership(0, group, &is_member);
			
			FreeSid(group);
			
			if (status == 0) {
				return -1;
			}
			
			return (int) is_member;
		#else
			return geteuid() == 0;
		#endif
		
	}
#endif

char* get_configuration_directory(void) {
	/*
	Returns the config directory of the current user for applications.
	
	On non-Windows OSs, this proc conforms to the XDG Base Directory
	spec. Thus, this proc returns the value of the "XDG_CONFIG_HOME" environment
	variable if it is set, otherwise it returns the default configuration directory ("~/.config/").
	
	Returns NULL on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const wchar_t* const wdirectory = _wgetenv(L"APPDATA");
			
			if (wdirectory == NULL) {
				return NULL;
			}
			
			const int directorys = WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, NULL, 0, NULL, NULL);
			
			if (directorys == 0) {
				return NULL;
			}
			
			char directory[(size_t) directorys];
			
			if (WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, directory, directorys, NULL, NULL) == 0) {
				return NULL;
			}
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
			
			char* configuration_directory = malloc(strlen(home_directory) + strlen(PATH_SEPARATOR) + strlen(config_directory) + 1);
			
			if (configuration_directory == NULL) {
				return NULL;
			}
			
			strcpy(configuration_directory, home_directory);
			strcat(configuration_directory, PATH_SEPARATOR);
			strcat(configuration_directory, config_directory);
			
			return configuration_directory;
		}
	#endif
	
	// Strip trailing path separator
	if (strlen(directory) > 1) {
		char* ptr = strchr(directory, '\0') - 1;
		
		if (*ptr == *PATH_SEPARATOR) {
			*ptr = '\0';
		}
	}
	
	char* configuration_directory = malloc(strlen(directory) + 1);
	
	if (configuration_directory == NULL) {
		return NULL;
	}
	
	strcpy(configuration_directory, directory);
	
	return configuration_directory;
	
}

char* get_temporary_directory(void) {
	/*
	Returns the temporary directory of the current user for applications to
	save temporary files in.
	
	On Windows, it calls GetTempPath().
	On Posix based platforms, it will check "TMPDIR", "TEMP", "TMP" and "TEMPDIR" environment variables in order.
	
	Returns NULL on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const size_t wdirectorys = (size_t) GetTempPathW(0, NULL);
			
			if (wdirectorys == 0) {
				return NULL;
			}
			
			wchar_t wdirectory[wdirectorys + 1];
			
			const DWORD code = GetTempPathW((DWORD) (sizeof(wdirectory) / sizeof(*wdirectory)), wdirectory);
			
			if (code == 0) {
				return 0;
			}
			
			const int directorys = WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, NULL, 0, NULL, NULL);
			
			if (directorys == 0) {
				return NULL;
			}
			
			char* temporary_directory = malloc((size_t) directorys);
			
			if (temporary_directory == NULL) {
				return NULL;
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, temporary_directory, directorys, NULL, NULL) == 0) {
				return NULL;
			}
		#else
			const size_t directorys = (size_t) GetTempPathA(0, NULL);
			
			if (directorys == 0) {
				return NULL;
			}
			
			char directory[directorys + 1];
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
			
			if (value == NULL) {
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
		
		char* temporary_directory = malloc(strlen(tmp) + 1);
		
		if (temporary_directory == NULL) {
			return NULL;
		}
		
		strcpy(temporary_directory, tmp);
	#endif
	
	// Strip trailing path separator
	if (strlen(temporary_directory) > 1) {
		char* ptr = strchr(temporary_directory, '\0') - 1;
		
		if (*ptr == *PATH_SEPARATOR) {
			*ptr = '\0';
		}
	}
	
	return temporary_directory;
	
}

char* get_home_directory(void) {
	/*
	Returns the home directory of the current user.
	
	On Windows, it returns the value from "USERPROFILE" environment variable.
	On Posix based platforms, it returns the value from "HOME" environment variable.
	
	Returns NULL on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const wchar_t* const wdirectory = _wgetenv(L"USERPROFILE");
			
			if (wdirectory == NULL) {
				return NULL;
			}
			
			const int directorys = WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, NULL, 0, NULL, NULL);
			
			if (directorys == 0) {
				return NULL;
			}
			
			char directory[(size_t) directorys];
			
			if (WideCharToMultiByte(CP_UTF8, 0, wdirectory, -1, directory, directorys, NULL, NULL) == 0) {
				return NULL;
			}
		#else
			const char* const directory = getenv("USERPROFILE");
			
			if (directory == NULL) {
				return NULL;
			}
		#endif
	#else
		const char* const directory = getenv("HOME");
		
		if (directory == NULL) {
			return NULL;
		}
	#endif
	
	if (*directory == '\0') {
		return NULL;
	}
	
	char* const home = malloc(strlen(directory) + 1);
	
	if (home == NULL) {
		return NULL;
	}
	
	strcpy(home, directory);
	
	return home;
	
}

char* find_exe(const char* const name) {
	
	#if defined(_WIN32) && defined(_UNICODE)
		const wchar_t* const wpath = _wgetenv(L"PATH");
		
		if (wpath == NULL) {
			return NULL;
		}
		
		const int paths = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
		
		if (paths == 0) {
			return NULL;
		}
		
		char path[(size_t) paths];
		
		if (WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, paths, NULL, NULL) == 0) {
			return NULL;
		}
	#else
		const char* const path = getenv("PATH");
		
		if (path == NULL) {
			return NULL;
		}
	#endif
	
	#ifdef _WIN32
		const char* const separator = SEMICOLON;
	#else
		const char* const separator = COLON;
	#endif
	
	const char* start = path;
	
	for (size_t index = 0; index < strlen(path) + 1; index++) {
		const char* const ch = &path[index];
		
		if (!(*ch == *separator || *ch == '\0')) {
			continue;
		}
		
		const size_t size = (size_t) (ch - start);
		
		char filename[size + strlen(PATH_SEPARATOR) + strlen(name) + strlen(EXECUTABLE_EXTENSION) + 1];
		memcpy(filename, start, size);
		filename[size] = '\0';
		
		strcat(filename, PATH_SEPARATOR);
		strcat(filename, name);
		strcat(filename, EXECUTABLE_EXTENSION);
		
		switch (file_exists(filename)) {
			case 1: {
				char* const executable = malloc(strlen(filename) + 1);
				
				if (executable == NULL) {
					return NULL;
				}
				
				strcpy(executable, filename);
				
				return executable;
			}
			case -1:
				return NULL;
		}
		
		start = ch + 1;
	}
	
	return NULL;
	
}
