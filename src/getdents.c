#if defined(__linux__)
	#define _GNU_SOURCE
#endif

#include <stdlib.h>

#if defined(__HAIKU__) || defined(__linux__) || defined(__APPLE__)
	#include <sys/types.h>
#endif

#if !defined(__HAIKU__)
	#include <fcntl.h>
	#include <unistd.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
	#include <dirent.h>
#endif

#if defined(__linux__)
	#include <unistd.h>
#endif

#if defined(__HAIKU__)
	#include <private/libroot/errno_private.h>
#endif

#include "getdents.h"

int open_dir(const char* const directory) {
	
	#ifdef __HAIKU__
		const int fd = _kern_open_dir(-1, directory);
		
		if (fd < 0) {
			__set_errno(fd);
			return -1;
		}
	#else
		const int fd = open(directory, O_RDONLY | O_DIRECTORY);
	#endif
	
	return fd;
	
}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
	ssize_t get_directory_entries(int fd, char* const buffer, const size_t buffer_size) {
		
		const ssize_t size = (ssize_t) getdents(fd, buffer, buffer_size);
		return size;
		
	}
#endif

#if defined(__linux__)
	ssize_t get_directory_entries(int fd, char* const buffer, const size_t buffer_size) {
		#if defined(SYS_getdents64)
			const long size = syscall(SYS_getdents64, fd, buffer, buffer_size);
		#else
			const long size = syscall(SYS_getdents, fd, buffer, buffer_size);
		#endif
		
		return (ssize_t) size;
	}
#endif

#if defined(__APPLE__)
	int getdirentries64(int, char*, int, long*) __asm("___getdirentries64");
	
	ssize_t get_directory_entries(int fd, char* const buffer, const size_t buffer_size) {
		
		long base = 0;
		const ssize_t size = (ssize_t) getdirentries64(fd, buffer, buffer_size, &base);
		
		return size;
		
	}
#endif

#if defined(__HAIKU__)
	int _kern_open_dir(int, const char*);
	ssize_t _kern_read_dir(int, struct dirent*, size_t, size_t);
	int _kern_close(int);
	
	ssize_t get_directory_entries(int fd, char* const buffer, const size_t buffer_size) {
		
		const ssize_t size = _kern_read_dir(fd, buffer, buffer_size, sizeof(*buffer));
		
		if (size < 0) {
			__set_errno(size);
			return -1;
		}
		
		return size;
		
	}
#endif

int close_dir(int fd) {
	
	#ifdef __HAIKU__
		const int status = _kern_close(fd);
		
		if (status < 0) {
			__set_errno(status);
			return -1;
		}
	#else
		const int status = close(fd);
	#endif
	
	return status;
	
}
