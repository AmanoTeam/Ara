#ifndef _WIN32
	#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <windows.h>
	#include <fileapi.h>
	#include <shlwapi.h>
#else
	#include <unistd.h>
	#include <sys/stat.h>
	#include <errno.h>
	#include <limits.h>
	#include <dirent.h>
#endif

#if !defined(_WIN32) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__DragonFly__) && !defined(__APPLE__) && !defined(__HAIKU__) && !defined(__OpenBSD__)
	#include <sys/syscall.h>
	
	struct linux_dirent {
		unsigned long d_ino;
		off_t d_off;
		unsigned short d_reclen;
		char d_name[];
	};
	
	#if defined(__aarch64__) || (defined(__riscv) && __LP64__)
		#define SYS_getdents SYS_getdents64
	#endif
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
	#include <sys/sysctl.h>
#endif

#if defined(__APPLE__)
	#include <sys/param.h>
	#include <copyfile.h>
	#include <mach-o/dyld.h>
	
	int getdirentries64(int fd, char *buf, int nbytes, long *basep) __asm("___getdirentries64");
#endif

#if defined(__HAIKU__)
	#include <FindDirectory.h>
	
	int _kern_open_dir(int fd, const char *path);
	ssize_t _kern_read_dir(int fd, struct dirent *buffer, size_t bufferSize, size_t maxCount);
#endif

#if !defined(__HAIKU__)
	#include <fcntl.h>
#endif

#include "fstream.h"
#include "symbols.h"
#include "filesystem.h"
#include "walkdir.h"

#ifdef _WIN32
	int is_absolute(const char* const path) {
		/*
		Checks whether a given path is absolute.
		
		On Windows, network paths are considered absolute too.
		*/
		
		#ifdef _WIN32
			return (*path == *PATH_SEPARATOR || (strlen(path) > 1 && isalpha(*path) && path[1] == *COLON));
		#else
			return (*path == *PATH_SEPARATOR);
		#endif
		
	}
#endif

char* get_current_directory(void) {
	/*
	Returns the current working directory.
	
	Returns NULL on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const DWORD size = GetCurrentDirectoryW(0, NULL);
			
			if (size == 0) {
				return NULL;
			}
			
			wchar_t wpwd[(size_t) size];
			const DWORD status = GetCurrentDirectoryW(size, wpwd);
			
			if (status == 0 || status > size) {
				return NULL;
			}
			
			const int pwds = WideCharToMultiByte(CP_UTF8, 0, wpwd, -1, NULL, 0, NULL, NULL);
			
			if (pwds == 0) {
				return NULL;
			}
			
			char* pwd = malloc((size_t) pwds);
			
			if (pwd == NULL) {
				return NULL;
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, wpwd, -1, pwd, pwds, NULL, NULL) == 0) {
				return NULL;
			}
		#else
			const DWORD size = GetCurrentDirectoryA(0, NULL);
			
			if (size == 0) {
				return NULL;
			}
			
			char cpwd[(size_t) size];
			const DWORD status = GetCurrentDirectoryA(size, cpwd);
			
			if (status == 0 || status > size) {
				return 0;
			}
			
			char* pwd = malloc((size_t) size);
			
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

int remove_file(const char* const filename) {
	/*
	Removes a file from disk.
	
	On Windows, ignores the read-only attribute.
	This does not fail if the file never existed in the first place.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int is_abs = is_absolute(filename);
			
			const int wfilenames = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			
			if (wfilenames == 0) {
				return -1;
			}
			
			wchar_t wfilename[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wfilenames];
			
			if (is_abs) {
				wcscpy(wfilename, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wfilenames) == 0) {
				return -1;
			}
			
			const BOOL status = DeleteFileW(wfilename);
		#else
			const BOOL status = DeleteFileA(filename);
		#endif
		
		if (status == 0) {
			if (GetLastError() == ERROR_ACCESS_DENIED) {
				#ifdef _UNICODE
					const BOOL status = SetFileAttributesW(wfilename, FILE_ATTRIBUTE_NORMAL);
				#else
					const BOOL status = SetFileAttributesA(filename, FILE_ATTRIBUTE_NORMAL);
				#endif
				
				if (status == 1) {
					#ifdef _UNICODE
						const BOOL status = DeleteFileW(wfilename);
					#else
						const BOOL status = DeleteFileA(filename);
					#endif
					
					if (status == 1) {
						return 0;
					}
				}
			} else if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
				return 0;
			}
			
			return -1;
		}
	#else
		if (unlink(filename) == -1) {
			if (errno == ENOENT) {
				return 0;
			}
			
			return -1;
		}
	#endif
	
	return 0;
	
}

static int remove_empty_directory(const char* const directory) {
	/*
	Deletes an existing empty directory.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int is_abs = is_absolute(directory);
			
			const int wdirectorys = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			
			if (wdirectorys == 0) {
				return -1;
			}
			
			wchar_t wdirectory[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wdirectorys];
			
			if (is_abs) {
				wcscpy(wdirectory, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, directory, -1, wdirectory + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wdirectorys) == 0) {
				return -1;
			}
			
			const BOOL status = RemoveDirectoryW(wdirectory);
		#else
			const BOOL status = RemoveDirectoryA(directory);
		#endif
		
		if (status == 0) {
			return -1;
		}
	#else
		if (rmdir(directory) == -1) {
			return -1;
		}
	#endif
	
	return 0;
	
}

int remove_recursive(const char* const directory, int remove_itself) {
	/*
	Recursively removes a directory from disk.
	
	Returns (0) on success, (-1) on error.
	*/
	
	struct WalkDir walkdir = {0};
	
	if (walkdir_init(&walkdir, directory) == -1) {
		return -1;
	}
	
	while (1) {
		const struct WalkDirItem* const item = walkdir_next(&walkdir);
		
		if (item == NULL) {
			break;
		}
		
		if (strcmp(item->name, ".") == 0 || strcmp(item->name, "..") == 0) {
			continue;
		}
		
		char path[strlen(directory) + strlen(PATH_SEPARATOR) + strlen(item->name) + 1];
		strcpy(path, directory);
		strcat(path, PATH_SEPARATOR);
		strcat(path, item->name);
		
		switch (item->type) {
			case WALKDIR_ITEM_DIRECTORY: {
				if (remove_recursive(path, 1) == -1) {
					walkdir_free(&walkdir);
					return -1;
				}
				
				break;
			}
			case WALKDIR_ITEM_FILE:
			case WALKDIR_ITEM_UNKNOWN: {
				if (remove_file(path) == -1) {
					walkdir_free(&walkdir);
					return -1;
				}
				
				break;
			}
		}
	}
	
	walkdir_free(&walkdir);
	
	if (remove_itself) {
		if (remove_empty_directory(directory) == -1) {
			return -1;
		}
	}
	
	return 0;
	
}

int directory_exists(const char* const directory) {
	/*
	Checks if directory exists.
	
	Returns (1) if directory exists, (0) if it does not exists, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int is_abs = is_absolute(directory);
			
			const int wdirectorys = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			
			if (wdirectorys == 0) {
				return -1;
			}
			
			wchar_t wdirectory[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wdirectorys];
			
			if (is_abs) {
				wcscpy(wdirectory, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, directory, -1, wdirectory + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wdirectorys) == 0) {
				return -1;
			}
			
			const DWORD value = GetFileAttributesW(wdirectory);
		#else
			const DWORD value = GetFileAttributesA(directory);
		#endif
		
		if (value == INVALID_FILE_ATTRIBUTES) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
				return 0;
			}
			
			return -1;
		}
		
		return (value & FILE_ATTRIBUTE_DIRECTORY) != 0;
	#else
		struct stat st = {0};
		
		if (stat(directory, &st) == -1) {
			if (errno == ENOENT) {
				return 0;
			}
			
			return -1;
		}
		
		return S_ISDIR(st.st_mode);
	#endif
	
}

int directory_empty(const char* const directory) {
	/*
	Determines whether a specified path is an empty directory.
	
	Returns (1) if directory is empty, (0) if it does not, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int is_abs = is_absolute(directory);
			
			const int wdirectorys = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			
			if (wdirectorys == 0) {
				return -1;
			}
			
			wchar_t wdirectory[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wdirectorys];
			
			if (is_abs) {
				wcscpy(wdirectory, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, directory, -1, wdirectory + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wdirectorys) == 0) {
				return -1;
			}
			
			const DWORD value = GetFileAttributesW(wdirectory);
		#else
			const DWORD value = GetFileAttributesA(directory);
		#endif
		
		if (value == INVALID_FILE_ATTRIBUTES) {
			return -1;
		}
		
		if ((value & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			return -1;
		}
		
		#ifdef _UNICODE
			const BOOL status = PathIsDirectoryEmptyW(wdirectory);
		#else
			const BOOL status = PathIsDirectoryEmptyA(directory);
		#endif
		
		return (int) status;
	#else
		#ifdef __HAIKU__
			const int fd = _kern_open_dir(-1, directory);
		#else
			const int fd = open(directory, O_RDONLY | O_DIRECTORY);
		#endif
		
		if (fd == -1) {
			return -1;
		}
		
		#if defined(__APPLE__) || defined(__HAIKU__)
			struct dirent buffer[3] = {'\0'};
		#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
			struct dirent buffer[2] = {'\0'};
		#else
			struct linux_dirent buffer[2] = {'\0'};
		#endif
		
		while (1) {
			#if defined(__APPLE__)
				long base = 0;
				const int size = getdirentries64(fd, (char*) buffer, sizeof(buffer), &base);
			#elif defined(__HAIKU__)
				const ssize_t size = _kern_read_dir(fd, buffer, sizeof(buffer), sizeof(buffer) / sizeof(*buffer));
			#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
				const ssize_t size = getdents(fd, (char*) buffer, sizeof(buffer));
			#else
				const long size = syscall(SYS_getdents, fd, buffer, sizeof(buffer));
			#endif
			
			if (size == 0) {
				close(fd);
				break;
			}
			
			#ifdef __HAIKU__
				if (size < 0) {
					return -1;
				}
			#else
				if (size == -1) {
					close(fd);
					
					if (errno == EINVAL) {
						return 0;
					}
					
					return -1;
				}
			#endif
			
			for (long index = 0; index < size;) {
				#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__APPLE__) || defined(__OpenBSD__)
					struct dirent* item = (struct dirent*) (((char*) buffer) + index);
				#elif defined(__HAIKU__)
					struct dirent* item = (struct dirent*) (buffer + index);
				#else
					struct linux_dirent* item = (struct linux_dirent*) (buffer + index);
				#endif
				
				if (!(strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0)) {
					close(fd);
					return 0;
				}
				
				#ifdef __DragonFly__
					index += _DIRENT_DIRSIZ(item);
				#else
					index += item->d_reclen;
				#endif
			}
		}
		
		return 1;
	#endif
	
}

int file_exists(const char* const filename) {
	/*
	Checks if file exists and is a regular file or symlink.
	
	Returns (1) if file exists, (0) if it does not exists, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int is_abs = is_absolute(filename);
			
			const int wfilenames = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			
			if (wfilenames == 0) {
				return -1;
			}
			
			wchar_t wfilename[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wfilenames];
			
			if (is_abs) {
				wcscpy(wfilename, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wfilenames) == 0) {
				return -1;
			}
			
			const DWORD value = GetFileAttributesW(wfilename);
		#else
			const DWORD value = GetFileAttributesA(filename);
		#endif
		
		if (value == INVALID_FILE_ATTRIBUTES) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND) {
				return 0;
			}
			
			return -1;
		}
		
		return (value & FILE_ATTRIBUTE_DIRECTORY) == 0;
	#else
		struct stat st = {0};
		
		if (stat(filename, &st) == -1) {
			if (errno == ENOENT) {
				return 0;
			}
			
			return -1;
		}
		
		return S_ISREG(st.st_mode);
	#endif
	
}

static int raw_create_dir(const char* const directory) {
	/*
	Try to create one directory (not the whole path).
	
	This is a thin wrapper over mkdir() (or alternatives on other systems), so in case of a pre-existing path we don't check that it is a directory.
	
	Returns (1) on success, (0) if it already exists, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			const int is_abs = is_absolute(directory);
			
			const int wdirectorys = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			
			if (wdirectorys == 0) {
				return -1;
			}
			
			wchar_t wdirectory[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wdirectorys];
			
			if (is_abs) {
				wcscpy(wdirectory, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, directory, -1, wdirectory + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wdirectorys) == 0) {
				return -1;
			}
			
			const BOOL status = CreateDirectoryW(wdirectory, NULL);
		#else
			const BOOL status = CreateDirectoryA(directory, NULL);
		#endif
		
		if (status == 0) {
			if (GetLastError() == ERROR_ALREADY_EXISTS) {
				return 0;
			}
			
			return -1;
		}
	#else
		if (mkdir(directory, 0777) == -1) {
			#ifdef __HAIKU__
				if (errno == EEXIST || errno == EROFS) {
					return 0;
				}
			#else
				if (errno == EEXIST) {
					return 0;
				}
			#endif
			
			return -1;
		}
	#endif
	
	return 1;
	
}

int create_directory(const char* const directory) {
	/*
	Creates the directory.
	
	The directory may contain several subdirectories that do not exist yet.
	The full path is created.
	
	It does not fail if the directory already exists because for
	most usages this does not indicate an error.
	
	Returns (0) on success, (-1) on error.
	*/
	
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
			
			if (raw_create_dir(directory) == -1) {
				return -1;
			}
		}
	}
	
	return 0;
	
}

int copy_file(const char* const source, const char* const destination) {
	/*
	Copies a file from source to destination.
	
	On the Windows platform this will copy the source file's attributes into destination.
	On Mac OS X, copyfile() C API will be used (available since OS X 10.5).
	
	If destination already exists, the file attributes will be preserved and the content overwritten.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
		#ifdef _UNICODE
			int is_abs = is_absolute(source);
			
			const int wsources = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
			
			if (wsources == 0) {
				return -1;
			}
			
			wchar_t wsource[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wsources];
			
			if (is_abs) {
				wcscpy(wsource, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, source, -1, wsource + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wsources) == 0) {
				return -1;
			}
			
			is_abs = is_absolute(destination);
			
			const int wdestinations = MultiByteToWideChar(CP_UTF8, 0, destination, -1, NULL, 0);
			
			if (wdestinations == 0) {
				return -1;
			}
			
			wchar_t wdestination[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wdestinations];
			
			if (is_abs) {
				wcscpy(wdestination, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, destination, -1, wdestination + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wdestinations) == 0) {
				return -1;
			}
			
			if (CopyFileW(wsource, wdestination, FALSE) == 0) {
				return -1;
			}
		#else
			if (CopyFileA(source, destination, FALSE) == 0) {
				return -1;
			}
		#endif
	#elif defined(__APPLE__)
		copyfile_state_t state = copyfile_state_alloc();
		
		const int status = copyfile(source, destination, state, COPYFILE_DATA);
		const int status2 = copyfile_state_free(state);
		
		if (status != 0 || status2 != 0) {
			return -1;
		}
	#else
		// Generic version which works for any platform
		struct FStream* const istream = fstream_open(source, FSTREAM_READ);
		
		if (istream == NULL) {
			return -1;
		}
		
		struct FStream* const ostream = fstream_open(destination, FSTREAM_WRITE);
		
		if (ostream == NULL) {
			fstream_close(istream);
			return -1;
		}
		
		char chunk[8192] = {'\0'};
		
		while (1) {
			const ssize_t size = fstream_read(istream, chunk, sizeof(chunk));
			
			if (size == -1) {
				fstream_close(istream);
				fstream_close(ostream);
				return -1;
			}
			
			if (size == 0) {
				if (fstream_close(istream) == -1 || fstream_close(ostream) == -1) {
					return -1;
				}
				break;
			}
			
			const int status = fstream_write(ostream, chunk, (size_t) size);
			
			if (status == -1) {
				fstream_close(istream);
				fstream_close(ostream);
				return -1;
			}
		}
	#endif
	
	return 0;

}

int move_file(const char* const source, const char* const destination) {
	/*
	Moves a file from source to destination.
	
	Symlinks are not followed: if source is a symlink, it is itself moved, not it's target.
	If destination already exists, it will be overwritten.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			int is_abs = is_absolute(source);
			
			const int wsources = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
			
			if (wsources == 0) {
				return -1;
			}
			
			wchar_t wsource[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wsources];
			
			if (is_abs) {
				wcscpy(wsource, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, source, -1, wsource + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wsources) == 0) {
				return -1;
			}
			
			is_abs = is_absolute(destination);
			
			const int wdestinations = MultiByteToWideChar(CP_UTF8, 0, destination, -1, NULL, 0);
			
			if (wdestinations == 0) {
				return -1;
			}
			
			wchar_t wdestination[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wdestinations];
			
			if (is_abs) {
				wcscpy(wdestination, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, destination, -1, wdestination + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wdestinations) == 0) {
				return -1;
			}
			
			const BOOL status = MoveFileExW(wsource, wdestination, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
		#else
			const BOOL status = MoveFileExA(source, destination, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
		#endif
		
		if (status == 0) {
			if (GetLastError() == ERROR_ACCESS_DENIED) {
				if (copy_file(source, destination) == -1) {
					return -1;
				}
				
				if (remove_file(source) == 0) {
					return 0;
				}
			}
			
			return -1;
		}
	#else
		if (rename(source, destination) == -1) {
			if (errno == EXDEV) {
				if (copy_file(source, destination) == -1) {
					return -1;
				}
				
				if (remove_file(source) == 0) {
					return 0;
				}
			}
			
			return -1;
		}
	#endif
	
	return 0;
	
}

char* get_app_filename(char* const filename) {
	/*
	Returns the filename of the application's executable.
	
	Returns NULL on error.
	*/
	
	#ifdef _WIN32
		#ifdef _UNICODE
			wchar_t wfilename[PATH_MAX];
			const DWORD code = GetModuleFileNameW(0, wfilename, (DWORD) (sizeof(wfilename) / sizeof(*wfilename)));
			
			if (code == 0 || code > (DWORD) (sizeof(wfilename) / sizeof(*wfilename))) {
				return NULL;
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, filename, PATH_MAX, NULL, NULL) == 0) {
				return NULL;
			}
		#else
			if (GetModuleFileNameA(0, filename, PATH_MAX) == 0) {
				return NULL;
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
		
		size_t size = PATH_MAX;
		
		if (sysctl(call, sizeof(call) / sizeof(*call), filename, &size, NULL, 0) == -1) {
			return NULL;
		}
	#elif defined(__OpenBSD__)
		const pid_t pid = getpid();
		
		const int call[] = {
			CTL_KERN,
			KERN_PROC_ARGS,
			pid,
			KERN_PROC_ARGV
		};
		
		size_t size = 0;
		
		if (sysctl(call, sizeof(call) / sizeof(*call), NULL, &size, NULL, 0) == -1) {
			return NULL;
		}
		
		char* argv[size / sizeof(char*)];
		
		if (sysctl(call, sizeof(call) / sizeof(*call), argv, &size, NULL, 0) == -1) {
			return NULL;
		}
		
		const char* const name = *argv;
		
		if (*name == *PATH_SEPARATOR) {
			// Path is absolute
			if (realpath(name, filename) == NULL) {
				return NULL;
			}
			
			return filename;
		}
		
		// Not an absolute path, check if it's relative to the current working directory
		int is_relative = 0;
		
		for (size_t index = 1; index < strlen(name); index++) {
			const char ch = name[index];
			
			is_relative = ch == *PATH_SEPARATOR;
			
			if (is_relative) {
				break;
			}
		}
		
		if (is_relative) {
			char cwd[PATH_MAX] = {'\0'};
			
			if (getcwd(cwd, sizeof(cwd)) == NULL) {
				return NULL;
			}
			
			char path[strlen(cwd) + strlen(PATH_SEPARATOR) + strlen(name) + 1];
			strcpy(path, cwd);
			strcat(path, PATH_SEPARATOR);
			strcat(path, name);
			
			if (realpath(path, filename) == NULL) {
				return NULL;
			}
			
			return filename;
		}
		
		// Search in PATH
		const char* const path = getenv("PATH");
		
		if (path == NULL) {
			return NULL;
		}
		
		const char* start = path;
		
		for (size_t index = 0; index < strlen(path) + 1; index++) {
			const char* const ch = &path[index];
			
			if (!(*ch == *COLON || *ch == '\0')) {
				continue;
			}
			
			const size_t size = (size_t) (ch - start);
			
			char executable[size + strlen(PATH_SEPARATOR) + strlen(name) + 1];
			memcpy(executable, start, size);
			executable[size] = '\0';
			
			strcat(executable, PATH_SEPARATOR);
			strcat(executable, name);
			
			switch (file_exists(executable)) {
				case 1: {
					if (realpath(executable, filename) == NULL) {
						return NULL;
					}
					
					return filename;
				}
				case -1: {
					return NULL;
				}
			}
			
			start = ch + 1;
		}
		
		errno = ENOENT;
		
		return NULL;
	#elif defined(__APPLE__)
		char path[PATH_MAX] = {'\0'};
		uint32_t paths = (uint32_t) sizeof(path);
		
		if (_NSGetExecutablePath(path, &paths) == -1) {
			return 0;
		}
		
		char resolved_path[PATH_MAX] = {'\0'};
		
		if (realpath(path, resolved_path) == NULL) {
			return NULL;
		}
		
		strcpy(filename, resolved_path);
	#elif defined(__HAIKU__)
		if (find_path(NULL, B_FIND_PATH_IMAGE_PATH, NULL, filename, PATH_MAX) != B_OK) {
			return NULL;
		}
	#else
		if (readlink("/proc/self/exe", filename, PATH_MAX) == -1) {
			return NULL;
		}
	#endif
	
	return filename;
	
}

long long get_file_size(const char* const filename) {
	/*
	Returns the size of a file (in bytes).
	
	Returns -1 on error.
	*/
	
	#ifdef _WIN32
		WIN32_FIND_DATA data = {0};
		
		#ifdef _UNICODE
			const int is_abs = is_absolute(filename);
			
			const int wfilenames = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			
			if (wfilenames == 0) {
				return -1;
			}
			
			wchar_t wfilename[(is_abs ? wcslen(WIN10LP_PREFIX) : 0) + wfilenames];
			
			if (is_abs) {
				wcscpy(wfilename, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename + (is_abs ? wcslen(WIN10LP_PREFIX) : 0), wfilenames) == 0) {
				return -1;
			}
			
			const HANDLE handle = FindFirstFileW(wfilename, &data);
		#else
			const HANDLE handle = FindFirstFileA(filename, &data);
		#endif
		
		if (handle == INVALID_HANDLE_VALUE) {
			return -1;
		}
		
		FindClose(handle);
		
		return (data.nFileSizeHigh * MAXDWORD) + data.nFileSizeLow;
	#else
		struct stat st = {0};
		
		if (stat(filename, &st) == -1) {
			return -1;
		}
		
		return st.st_size;
	#endif
	
}
