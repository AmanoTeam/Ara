#include <stdlib.h>

#ifdef _WIN32
	#include <windows.h>
	#include <stdio.h>
	
	#define NAME_MAX (_MAX_FNAME - 1)
#else
	#include <limits.h>
	#include <dirent.h>
#endif

enum WalkDirType {
	WALKDIR_ITEM_DIRECTORY,
	WALKDIR_ITEM_FILE,
	WALKDIR_ITEM_UNKNOWN
};

struct WalkDirItem {
	enum WalkDirType type;
	size_t index;
	char name[NAME_MAX + 1];
};

struct WalkDir {
	struct WalkDirItem item;
#ifdef _WIN32
	HANDLE handle;
	WIN32_FIND_DATA data;
#else
	DIR* dir;
#endif
#ifdef __HAIKU__
	const char* directory;
#endif
};

int walkdir_init(struct WalkDir* const walkdir, const char* const directory);
const struct WalkDirItem* walkdir_next(struct WalkDir* const walkdir);
void walkdir_free(struct WalkDir* const walkdir);

#pragma once
