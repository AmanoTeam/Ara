#include <stdlib.h>

#if defined(_WIN32)
	#include <windows.h>
	#include <fileapi.h>
#endif

#if !defined(_WIN32)
	#include <stdio.h>
#endif

#include "fstream.h"

#if defined(_WIN32) && defined(_UNICODE)
	#include "symbols.h"
#endif

/*
fseeko()/ftello() availability:

On Linux: available when defined(_LARGEFILE64_SOURCE) or _POSIX_C_SOURCE >= 200112L
On Apple, Android, Haiku and SerenityOS: available by default
On FreeBSD and DragonFly BSD: available when __POSIX_VISIBLE >= 200112 or __XSI_VISIBLE >= 500
On NetBSD: available when __POSIX_VISIBLE >= 200112 or __XSI_VISIBLE >= 500 or defined(_NETBSD_SOURCE)
*/

#if ((defined(__linux__) && (defined(_LARGEFILE64_SOURCE) || _POSIX_C_SOURCE >= 200112L)) || defined(__HAIKU__) || defined(__serenity__) || \
	((defined(__FreeBSD__) || defined(__DragonFly__)) && (__POSIX_VISIBLE >= 200112 || __XSI_VISIBLE >= 500)) || \
	defined(__OpenBSD__) || (defined(__NetBSD__) && (__POSIX_VISIBLE >= 200112 || __XSI_VISIBLE >= 500 || defined(_NETBSD_SOURCE))) || \
	defined(__ANDROID__) || defined(__APPLE__))
	#define HAVE_FSEEKO 1
	#define HAVE_FTELLO 1
#endif

struct FStream* fstream_open(const char* const filename, const enum FStreamMode mode) {
	/*
	Opens a file on disk.
	
	Returns a null pointer on error.
	*/
	
	#if defined(_WIN32)
		DWORD desired_access = 0;
		DWORD creation_disposition = 0;
		const DWORD flags_and_attributes = FILE_ATTRIBUTE_NORMAL;
		
		switch (mode) {
			case FSTREAM_WRITE:
				desired_access |= GENERIC_WRITE;
				creation_disposition |= CREATE_ALWAYS;
				break;
			case FSTREAM_READ:
				desired_access |= GENERIC_READ;
				creation_disposition |= OPEN_EXISTING;
				break;
			case FSTREAM_APPEND:
				desired_access |= FILE_APPEND_DATA;
				creation_disposition |= OPEN_EXISTING;
				break;
		}
		
		#if defined(_UNICODE)
			const int wcsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
			
			if (wcsize == 0) {
				return NULL;
			}
			
			wchar_t name[wcslen(WIN10LP_PREFIX) + wcsize];
			wcscpy(name, WIN10LP_PREFIX);
			
			if (MultiByteToWideChar(CP_UTF8, 0, filename, -1, name + wcslen(WIN10LP_PREFIX), wcsize) == 0) {
				return NULL;
			}
			
			HANDLE handle = CreateFileW(
				name,
				desired_access,
				0,
				NULL,
				creation_disposition,
				flags_and_attributes,
				NULL
			);
		#else
			HANDLE handle = CreateFileA(
				filename,
				desired_access,
				0,
				NULL,
				creation_disposition,
				flags_and_attributes,
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
		const char* fmode = NULL;
		
		switch (mode) {
			case FSTREAM_WRITE:
				fmode = "wb";
				break;
			case FSTREAM_READ:
				fmode = "r";
				break;
			case FSTREAM_APPEND:
				fmode = "a";
				break;
		}
		
		FILE* handle = fopen(filename, fmode);
		
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
	Reads a block of data.
	
	Returns (>=1) on success, (0) on EOF, (-1) on error.
	*/
	
	#if defined(_WIN32)
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
	/*
	Writes a block of data.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
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
	/*
	Sets the current file position.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
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
		
		#if defined(HAVE_FSEEKO)
			if (fseeko(stream->stream, offset, whence) != 0) {
				return -1;
			}
		#else
			if (fseek(stream->stream, offset, whence) != 0) {
				return -1;
			}
		#endif
	#endif
	
	return 0;
	
}

long int fstream_tell(struct FStream* const stream) {
	/*
	Returns the current file offset.
	
	Returns (>=0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
		const DWORD value = SetFilePointer(stream->stream, 0, NULL, FILE_CURRENT);
		
		if (value == INVALID_SET_FILE_POINTER) {
			return -1;
		}
	#else
		#if defined(HAVE_FTELLO)
			const long int value = ftello(stream->stream);
		#else
			const long int value = ftell(stream->stream);
		#endif
		
		if (value == -1) {
			return -1;
		}
	#endif
	
	return value;
	
}

int fstream_close(struct FStream* const stream) {
	/*
	Closes the stream.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
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
