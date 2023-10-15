#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "uri.h"

static const char QUOTE_SAFE_SYMBOLS[] = "-._~";
static const char REQUOTE_SAFE_SYMBOLS[] = "!#$%&'()*+,/:;=?@[]~";

static char to_hex(const char ch) {
	return ch + (ch > 9 ? ('A' - 10) : '0');
}

static const char* urlencode(const unsigned char source, char* const destination) {
	
	if (isalnum(source) || strchr(QUOTE_SAFE_SYMBOLS, source) != NULL) {
		const char tmp[] = {source, '\0'};
		strcpy(destination, tmp);
	} else if (source == ' '){
		const char tmp[] = {'%', '2', '0', '\0'};
		strcpy(destination, tmp);
	} else {
		const char tmp[] = {'%', to_hex(source >> 4), to_hex(source % 16), '\0'};
		strcpy(destination, tmp);
	}

	return destination;
	
}

size_t requote_uri(const char* const source, char* const destination) {
	/*
	Re-quote the given URI. 
	
	This function passes the given URI through an unquote/quote cycle to 
	ensure that it is fully and consistently quoted.
	*/
	
	size_t required_size = 0;
	char tmp[4] = {'\0'};
	
	if (destination != NULL) {
		destination[0] = '\0';
	}
	
	for (size_t index = 0; index < strlen(source); index++) {
		const char ch = source[index];
		
		if (isalnum(ch) || strchr(REQUOTE_SAFE_SYMBOLS, ch) != NULL){
			strncat(tmp, &ch, 1);
		} else {
			urlencode(ch, tmp);
		}
		
		required_size += strlen(tmp);
		
		if (destination != NULL) {
			strcat(destination, tmp);
		}
		
		tmp[0] = '\0';
	}
	
	required_size++;
	
	return required_size;
	
}
