#include <stdlib.h>

#ifdef _WIN32
	#include <windows.h>
	#include <fileapi.h>
#else
	#include <stdio.h>
#endif

#include "fstream.h"

#if defined(_WIN32) && defined(_UNICODE)
	#include "symbols.h"
#endif

struct FStream* fstream_open(const char* const filename, const enum FStreamMode mode) {
	
	#ifdef _WIN32
		DWORD dwDesiredAccess = 0;
		DWORD dwCreationDisposition = 0;
		const DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
		
		switch (mode) {
			case FSTREAM_WRITE:
				dwDesiredAccess |= GENERIC_WRITE;
				dwCreationDisposition |= CREATE_ALWAYS;
				break;
			case FSTREAM_READ:
				dwDesiredAccess |= GENERIC_READ;
				dwCreationDisposition |= OPEN_EXISTING;
				break;
			case FSTREAM_APPEND:
				dwDesiredAccess |= FILE_APPEND_DATA;
				dwCreationDisposition |= OPEN_EXISTING;
				break;
		}
		
		#ifdef _UNICODE
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			
			if (wcsize == 0) {
				return NULL;
			}
			
			wchar_t lpFileName[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(lpFileName, WIN10LP_PREFIX);
			
			if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, lpFileName + wcslen(WIN10LP_PREFIX), wcsize) == 0) {
				return NULL;
			}
			
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
		
		if (mode == FSTREAM_APPEND) {
			if (SetFilePointer(handle, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
				CloseHandle(handle);
				return NULL;
			}
		}
	#else
		const char* smode = NULL;
		
		switch (mode) {
			case FSTREAM_WRITE:
				smode = "wb";
				break;
			case FSTREAM_READ:
				smode = "r";
				break;
			case FSTREAM_APPEND:
				smode = "a";
				break;
		}
		
		FILE* handle = fopen(filename, smode);
		
		if (handle == NULL) {
			return NULL;
		}
	#endif
	
	struct FStream* const stream = (struct FStream*) malloc(sizeof(struct FStream));
	
	if (stream == NULL) {
		return NULL;
	}
	
	stream->stream = handle;
	
	return stream;
	
}

ssize_t fstream_read(struct FStream* const stream, char* const buffer, const size_t size) {
	/*
	-1 = Read error
	0 = EOF reached
	1 = Read success
	*/
	
	#ifdef _WIN32
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
	
	#ifdef _WIN32
		DWORD wsize = 0;
		const BOOL status = WriteFile(stream->stream, buffer, (DWORD) size, &wsize, NULL);
		
		if (status == 0 || wsize != (DWORD) size) {
			return -1;
		}
	#else
		const size_t wsize = fwrite(buffer, sizeof(*buffer), size, stream->stream);
		
		if (wsize != size) {
			return -1;
		}
	#endif
	
	return 0;
	
}

int fstream_seek(struct FStream* const stream, const long int offset, const enum FStreamSeek method) {
	
	#ifdef _WIN32
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
			return -1;
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
			return -1;
		}
	#endif
	
	return 0;
	
}
	
int fstream_close(struct FStream* const stream) {
	
	#ifdef _WIN32
		if (stream->stream != 0) {
			const BOOL status = CloseHandle(stream->stream);
			
			if (status == 0) {
				return -1;
			}
			
			stream->stream = 0;
		}
	#else
		if (stream->stream != NULL) {
			if (fclose(stream->stream) != 0) {
				return -1;
			}
			
			stream->stream = NULL;
		}
	#endif
	
	free(stream);
	
	return 0;
	
}
