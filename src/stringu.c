#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
	#define NAME_MAX (_MAX_FNAME - 1)
#endif

#if !defined(_WIN32)
	#include <limits.h>
	#include <sys/types.h>
#endif

#include "stringu.h"
#include "symbols.h"

static const char INVALID_FILENAME_CHARS[] = {
	'\'', '%', '"', ' ', '/', '\\', ':', '*', '?', '\"', '<', '>', '|', '^', '\x00'
};

char* basename(const char* const path) {
	/*
	Returns the final component of a path.
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
	/*
	Returns the extension of a filename.
	*/
	
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
	
	if (*start == '\0') {
		return NULL;
	}
	
	for (size_t index = 0; index < strlen(start); index++) {
		const char ch = start[index];
		
		if (!isalnum(ch)) {
			return NULL;
		}
	}
	
	return start;
	
}

char* remove_file_extension(char* const filename) {
	/*
	Removes extension of the filename.
	*/
	
	while (1) {
		char* const file_extension = get_file_extension(filename);
		
		if (file_extension == NULL) {
			break;
		}
		
		*(file_extension - 1) = '\0';
	}
	
	return filename;
	
}

char* normalize_filename(char* filename) {
	/*
	Normalize filename by replacing invalid characters with underscore.
	*/
	
	for (size_t index = 0; index < strlen(filename); index++) {
		char* const ch = &filename[index];
		
		if (iscntrl((unsigned char) *ch) || strchr(INVALID_FILENAME_CHARS, *ch) != NULL) {
			*ch = *UNDERSCORE;
		}
	}
	
	char* start = filename;
	char* end = strchr(start, '\0');
	
	char* position = end;
	
	position--;
	
	while (position != start) {
		if (*position != *DOT) {
			break;
		}
		
		*position = '\0';
		position--;
	}
	
	position = start;
	
	while (position != end) {
		if (*position != *DOT) {
			break;
		}
		
		*position = '\0';
		position++;
	}
	
	if (position != start) {
		memmove(start, position, strlen(position) + 1);
	}
	
	return filename;
	
}

char* strip(char* const s) {
	/*
	Strips leading and trailing whitespaces from string.
	*/
	
	char* start = s;
	char* end = strchr(start, '\0');
	
	char* position = end;
	
	position--;
	
	while (position != start) {
		if (!(iscntrl((unsigned char) *position) || isspace((unsigned char) *position))) {
			break;
		}
		
		*position = '\0';
		position--;
	}
	
	position = start;
	
	while (position != end) {
		if (!(iscntrl((unsigned char) *position) || isspace((unsigned char) *position))) {
			break;
		}
		
		*position = '\0';
		position++;
	}
	
	if (position != start) {
		memmove(start, position, strlen(position) + 1);
	}
	
	return s;
	
}

char* normalize_directory(char* directory) {
	/*
	Normalize directory name by replacing invalid characters with underscore.
	*/
	
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
	
	return '0';
	
}

size_t intlen(const int value) {
	/*
	Calculates how many digits are required to represent this integer as a string.
	*/
	
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

char* get_parent_directory(const char* const source, char* const destination, const size_t depth) {
	/*
	Returns the parent directory of a path.
	*/
	
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
		
		if (index == 0 && len > 2 && isalpha((unsigned char) *source) && source[1] == *COLON && source[2] == *PATH_SEPARATOR) {
			memcpy(destination, source, 3);
			destination[3] = '\0';
		}
	}
	
	return destination;
	
}

int hashs(const char* const s) {
	/*
	Computes a numeric hash from a string.
	*/
	
	int value = 0;
	
	for (size_t index = 0; index < strlen(s); index++) {
		const int ch = s[index];
		value += ch + (int) index;
	}
	
	return value;
	
}