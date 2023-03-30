#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <dirent.h>
#endif

#ifdef __Haiku__
	#include <sys/stat.h>
#endif

#include "walkdir.h"
#include "symbols.h"

#ifdef _WIN32
	#include "filesystem.h"
#endif

int walkdir_init(struct WalkDir* const walkdir, const char* const directory) {
	
	#ifdef _WIN32
		#ifdef UNICODE
			const int absolute = is_absolute(directory);
			
			const int wpatterns = MultiByteToWideChar(CP_UTF8, 0, directory, -1, NULL, 0);
			
			if (wpatterns == 0) {
				return -1;
			}
			
			const int wpath_separators = MultiByteToWideChar(CP_UTF8, 0, PATH_SEPARATOR, -1, NULL, 0);
			
			if (wpath_separators == 0) {
				return -1;
			}
			
			wchar_t wpath_separator[wpath_separators];
			
			if (MultiByteToWideChar(CP_UTF8, 0, PATH_SEPARATOR, -1, wpath_separator, wpath_separators) == 0) {
				return -1;
			}
			
			const int wasterisks = MultiByteToWideChar(CP_UTF8, 0, ASTERISK, -1, NULL, 0);
			
			if (wasterisks == 0) {
				return -1;
			}
			
			wchar_t wasterisk[wasterisks];
			
			if (MultiByteToWideChar(CP_UTF8, 0, ASTERISK, -1, wasterisk, wasterisks) == 0) {
				return -1;
			}
			
			wchar_t wpattern[(absolute ? wcslen(WIN10LP_PREFIX) : 0) + wpatterns + wcslen(wpath_separator) + wcslen(wasterisk)];
			
			if (absolute) {
				wcscpy(wpattern, WIN10LP_PREFIX);
			}
			
			if (MultiByteToWideChar(CP_UTF8, 0, directory, -1, wpattern + (absolute ? wcslen(WIN10LP_PREFIX) : 0), wpatterns) == 0) {
				return -1;
			}
			
			wcscat(wpattern, wpath_separator);
			wcscat(wpattern, wasterisk);
			
			walkdir->handle = FindFirstFileW(wpattern, &walkdir->data);
		#else
			char pattern[strlen(directory) + strlen(PATH_SEPARATOR) + strlen(ASTERISK) + 1];
			strcpy(pattern, directory);
			strcat(pattern, PATH_SEPARATOR);
			strcat(pattern, ASTERISK);
			
			walkdir->handle = FindFirstFileA(pattern, &walkdir->data);
		#endif
		
		if (walkdir->handle == INVALID_HANDLE_VALUE) {
			return -1;
		}
	#else
		walkdir->dir = opendir(directory);
		
		if (walkdir->dir == NULL) {
			return -1;
		}
		
		#ifdef __Haiku__
			walkdir->directory = directory;
		#endif
	#endif
	
	return 0;
	
}

const struct WalkDirItem* walkdir_next(struct WalkDir* const walkdir) {
	
	#ifdef _WIN32
		#ifdef UNICODE
			if (walkdir->item.index > 0) {
				if (FindNextFileW(walkdir->handle, &walkdir->data) == 0) {
					return NULL;
				}
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, walkdir->data.cFileName, -1, walkdir->item.name, sizeof(walkdir->item.name), NULL, NULL) == 0) {
				return NULL;
			}
		#else
			if (walkdir->item.index > 0) {
				if (FindNextFileA(walkdir->handle, &walkdir->data) == 0) {
					return NULL;
				}
			}
			
			if (strlen(walkdir->data.cFileName) > (sizeof(walkdir->item.name) -1)) {
				return NULL;
			}
			
			strcpy(walkdir->item.name, walkdir->data.cFileName);
		#endif
		
		if ((walkdir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0) {
			walkdir->item.type = WALKDIR_ITEM_DIRECTORY;
		} else {
			walkdir->item.type = WALKDIR_ITEM_FILE;
		}
	#else
		const struct dirent* const item = readdir(walkdir->dir);
		
		if (item == NULL) {
			return NULL;
		}
		
		if (strlen(item->d_name) > (sizeof(walkdir->item.name) -1)) {
			return NULL;
		}
		
		strcpy(walkdir->item.name, item->d_name);
		
		#ifdef __Haiku__
			char path[strlen(walkdir->directory) + strlen(PATH_SEPARATOR) + strlen(item->d_name) + 1];
			strcpy(path, walkdir->directory);
			strcat(path, PATH_SEPARATOR);
			strcat(path, item->d_name);
			
			struct stat st = {0};
			
			if (stat(path, &st) == -1) {
				return NULL;
			}
		#endif
		
		#ifdef __Haiku__
			switch (st.st_mode & S_IFMT) {
				case S_IFDIR:
				case S_IFBLK:
					walkdir->item.type = WALKDIR_ITEM_DIRECTORY;
					break;
				case S_IFLNK:
				case S_IFIFO:
				case S_IFREG:
				case S_IFSOCK:
				case S_IFCHR:
					walkdir->item.type = WALKDIR_ITEM_FILE;
					break;
				default:
					walkdir->item.type = WALKDIR_ITEM_UNKNOWN;
					break;
			}
		#else
			switch (item->d_type) {
				case DT_DIR:
				case DT_BLK:
					walkdir->item.type = WALKDIR_ITEM_DIRECTORY;
					break;
				case DT_LNK:
				case DT_FIFO:
				case DT_REG:
				case DT_SOCK:
				case DT_CHR:
					walkdir->item.type = WALKDIR_ITEM_FILE;
					break;
				case DT_UNKNOWN:
					walkdir->item.type = WALKDIR_ITEM_UNKNOWN;
					break;
			}
		#endif
	#endif
	
	walkdir->item.index++;
	
	return &walkdir->item;
	
}

void walkdir_free(struct WalkDir* const walkdir) {
	
	#ifdef _WIN32
		FindClose(walkdir->handle);
	#else
		closedir(walkdir->dir);
	#endif
	
}
