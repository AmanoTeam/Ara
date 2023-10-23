#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "symbols.h"
#include "readlines.h"

void readlines_init(struct ReadLines* const readlines, const char* const s) {
	
	readlines->s = s;
	readlines->send = strchr(readlines->s, '\0');
	
	readlines->lbegin = readlines->s;
	readlines->lend = strstr(readlines->lbegin, LF);
	
}

const struct Line* readlines_next(struct ReadLines* const readlines, struct Line* const line) {
	
	if (readlines->lend == NULL) {
		readlines->lend = readlines->send;
	}
	
	const size_t line_size = (size_t) (readlines->lend - readlines->lbegin);
	
	if (line_size > 0) {
		const char* start = readlines->lbegin;
		const char* end = readlines->lend;
		
		while (start != end) {
			const char ch = *start;
			
			if (!isspace(ch)) {
				break;
			}
			
			start++;
		}
		
		end--;
		
		while (end != start) {
			if (!isspace(*end)) {
				break;
			}
			
			end--;
		}
		
		if (end != start) {
			line->begin = start;
			line->size = ((size_t) (end - start)) + 1;
		}
	}
	
	if (readlines->lend == readlines->send) {
		return NULL;
	}
	
	if (readlines->lbegin != readlines->s) {
		line->index++;
	}
	
	readlines->lbegin = readlines->lend;
	readlines->lbegin++;
	
	readlines->lend = strstr(readlines->lbegin, LF);
	
	return line;
	
}
		