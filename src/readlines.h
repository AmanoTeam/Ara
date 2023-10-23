struct ReadLines {
	const char* s;
	const char* send;
	const char* lbegin;
	const char* lend;
};

struct Line {
	size_t index;
	size_t size;
	const char* begin;
};

void readlines_init(struct ReadLines* const readlines, const char* const s);
const struct Line* readlines_next(struct ReadLines* const readlines, struct Line* const line);

#pragma once