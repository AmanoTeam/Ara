#include <stdlib.h>

#if defined(__linux__)
	#include <features.h>
	
	#if !defined(__USE_GNU)
		#define __MUSL__ 1
	#endif
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || \
	defined(__OpenBSD__) || defined(__HAIKU__) || defined(__APPLE__) || \
	(defined(__linux__) && (defined(__GLIBC__) || defined(__MUSL__)))
	#include <dirent.h>
	
	#if defined(__GLIBC__) && defined(_LARGEFILE64_SOURCE)
		#define directory_entry dirent64
	#else
		#define directory_entry dirent
	#endif
#endif

#if defined(__linux__) && !(defined(__GLIBC__) || defined(__MUSL__))
	#include <sys/syscall.h>
	#include <sys/types.h>
	
	#if defined(SYS_getdents64)
		struct linux_dirent64 {
			ino64_t d_ino;
			off64_t d_off;
			unsigned short d_reclen;
			unsigned char  d_type;
			char d_name[];
		};
		
		#define directory_entry linux_dirent64
	#else
		struct linux_dirent {
			unsigned long d_ino;
			off_t d_off;
			unsigned short d_reclen;
			char d_name[];
		};
		
		#define directory_entry linux_dirent
	#endif
#endif

#if !defined(directory_entry)
	#error "This platform lacks a functioning getdents/getdirentries implementation"
#endif

int open_dir(const char* const directory);
ssize_t get_directory_entries(int fd, char* const buffer, const size_t buffer_size);
int close_dir(int fd);

#if defined(__DragonFly__)
	#define directory_entry_size(entry) _DIRENT_DIRSIZ(entry)
#else
	#define directory_entry_size(entry) (entry->d_reclen)
#endif

#pragma once
