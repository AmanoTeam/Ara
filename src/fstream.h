#include <stdlib.h>

#ifdef WIN32
	#include <windows.h>
	
	struct FStream {
		HANDLE stream;
	};
#else
	#include <stdio.h>
	
	struct FStream {
		FILE* stream;
	};
#endif

enum FStreamSeek {
	FSTREAM_SEEK_BEGIN,
	FSTREAM_SEEK_CURRENT,
	FSTREAM_SEEK_END
};

struct FStream* fstream_open(const char* const filename, const char* const mode);
int fstream_write(struct FStream* stream, const char* const buffer, const size_t size);
int fstream_seek(struct FStream* stream, const long int offset, const enum FStreamSeek method);
int fstream_close(struct FStream* stream);

#pragma once