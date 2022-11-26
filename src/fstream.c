#include <stdlib.h>

#ifdef WIN32
	#include <windows.h>
	#include <fileapi.h>
#else
	#include <stdio.h>
#endif

#include "fstream.h"

struct FStream* fstream_open(const char* const filename, const char* const mode) {
	
	#ifdef WIN32
		DWORD dwDesiredAccess = 0;
		DWORD dwCreationDisposition = 0;
		const DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
		
		switch (*mode) {
			case 'w':
				dwDesiredAccess |= GENERIC_WRITE;
				dwCreationDisposition |= CREATE_ALWAYS;
				break;
			case 'r':
				dwDesiredAccess |= GENERIC_READ;
				dwCreationDisposition |= OPEN_EXISTING;
				break;
			default:
				return NULL;
		}
		
		#ifdef UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			wchar_t lpFileName[wcsize];
			MultiByteToWideChar(CP_UTF8, 0, filename, -1, lpFileName, sizeof(lpFileName) / sizeof(*lpFileName));
			
			HANDLE handle = CreateFileW(
				lpFileName,
				dwDesiredAccess,
				0,
				NULL,
				dwCreationDisposition,
				dwFlagsAndAttributes,
				NULL
			);
		#else
			HANDLE handle = CreateFileA(
				filename,
				dwDesiredAccess,
				0,
				NULL,
				dwCreationDisposition,
				dwFlagsAndAttributes,
				NULL
			);
		#endif
		
		if (handle == INVALID_HANDLE_VALUE) {
			return NULL;
		}
	#else
		FILE* handle = fopen(filename, mode);
		
		if (handle == NULL) {
			return NULL;
		}
	#endif
	
	struct FStream* const stream = malloc(sizeof(struct FStream));
	
	if (stream == NULL) {
		return NULL;
	}
	
	stream->stream = handle;
	
	return stream;
	
}

ssize_t fstream_read(struct FStream* const stream, char* const buffer, const size_t size) {
	/*
	-1 Read error
	0 EOF reached
	1 Read success
	*/
	
	#ifdef WIN32
		DWORD rsize = 0;
		const BOOL status = ReadFile(stream->stream, buffer, (DWORD) size, &rsize, NULL);
		
		if (!status) {
			return -1;
		}
		
		return (rsize > 0) ? (ssize_t) rsize : 0;
	#else
		const size_t rsize = fread(buffer, sizeof(*buffer), size, stream->stream);
		
		if (rsize == 0) {
			if (ferror(stream->stream) != 0) {
				return -1;
			}
			
			return 0;
		}
		
		return (ssize_t) rsize;
	#endif
	
}

int fstream_write(struct FStream* const stream, const char* const buffer, const size_t size) {
	
	#ifdef WIN32
		DWORD wsize = 0;
		const BOOL status = WriteFile(stream->stream, buffer, (DWORD) size, &wsize, NULL);
		
		if (status == 0 || wsize != (DWORD) size) {
			return 0;
		}
	#else
		const size_t wsize = fwrite(buffer, sizeof(*buffer), size, stream->stream);
		
		if (wsize != size) {
			return 0;
		}
	#endif
	
	return 1;
	
}

int fstream_seek(struct FStream* const stream, const long int offset, const enum FStreamSeek method) {
	
	#ifdef WIN32
		DWORD whence = 0;
		
		switch (method) {
			case FSTREAM_SEEK_BEGIN:
				whence = FILE_BEGIN;
				break;
			case FSTREAM_SEEK_CURRENT:
				whence = FILE_CURRENT;
				break;
			case FSTREAM_SEEK_END:
				whence = FILE_END;
				break;
		}
		
		if (SetFilePointer(stream->stream, offset, NULL, whence) == INVALID_SET_FILE_POINTER) {
			return 0;
		}
	#else
		int whence = 0;
		
		switch (method) {
			case FSTREAM_SEEK_BEGIN:
				whence = SEEK_SET;
				break;
			case FSTREAM_SEEK_CURRENT:
				whence = SEEK_CUR;
				break;
			case FSTREAM_SEEK_END:
				whence = SEEK_END;
				break;
		}
		
		if (fseek(stream->stream, offset, whence) != 0) {
			return 0;
		}
	#endif
	
	return 1;
	
}
	
int fstream_close(struct FStream* const stream) {
	
	#ifdef WIN32
		if (stream->stream != 0) {
			const BOOL status = CloseHandle(stream->stream);
			
			if (status == 0) {
				return 0;
			}
			
			stream->stream = 0;
		}
	#else
		if (stream->stream != NULL) {
			if (fclose(stream->stream) != 0) {
				return 0;
			}
			
			stream->stream = NULL;
		}
	#endif
	
	free(stream);
	
	return 1;
	
}