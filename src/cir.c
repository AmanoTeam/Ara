#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
	#include <windows.h>
	#include <conio.h>
#endif

#if !defined(_WIN32)
	#include <termios.h>
	#include <unistd.h>
#endif

#include "cir.h"

int cir_init(struct CIR* const obj) {
	/*
	Initializes the Console Input Reader struct.
	
	On Linux, this will set the standard input to raw mode.
	Under Windows, this will remove the ENABLE_PROCESSED_INPUT bit from the standard input flags.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
		const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
		
		if (handle == INVALID_HANDLE_VALUE) {
			return -1;
		}
		
		DWORD attributes = 0;
		
		if (GetConsoleMode(handle, &attributes) == 0) {
			return -1;
		}
		
		obj->attributes = attributes;
		
		attributes &= (DWORD) ~ENABLE_PROCESSED_INPUT;
		
		if (SetConsoleMode(handle, attributes) == 0) {
			return -1;
		}
	#else
		const int fd = fileno(stdin);
		
		if (fd == -1) {
			return -1;
		}
		
		struct termios attributes = {0};
		
		if (tcgetattr(fd, &attributes) == -1) {
			return -1;
		}
		
		obj->attributes = attributes;
		
		cfmakeraw(&attributes);
		
		attributes.c_cc[VTIME] = 0;
		attributes.c_cc[VMIN] = 1;
		
		int actions = TCSAFLUSH;
		
		#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
			actions |= TCSASOFT;
		#endif
		
		if (tcsetattr(fd, actions, &attributes) == -1) {
			return -1;
		}
		
		fflush(stdin);
	#endif
	
	return 0;
	
}

const struct CIKey* cir_get(struct CIR* const obj) {
	/*
	Listen for key presses on keyboard.
	*/
	
	memset(obj->tmp, '\0', sizeof(obj->tmp));
	
	#if defined(_WIN32)
		const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
		
		if (handle == INVALID_HANDLE_VALUE) {
			return NULL;
		}
	#endif
	
	#if defined(_WIN32)
		INPUT_RECORD input = {0};
		DWORD count = 0;
		
		#if defined(_UNICODE)
			const BOOL status = ReadConsoleInputW(handle, &input, 1, &count);
		#else
			const BOOL status = ReadConsoleInputA(handle, &input, 1, &count);
		#endif
		
		if (status == 0) {
			return NULL;
		}
		
		const KEY_EVENT_RECORD* const event = (KEY_EVENT_RECORD*) &input.Event;
		
		if (!(input.EventType == KEY_EVENT && event->bKeyDown)) {
			return &KEYBOARD_KEY_EMPTY;
		}
		
		#if defined(_UNICODE)
			if (event->uChar.UnicodeChar == L'\0') {
				obj->tmp[0] = (char) event->wVirtualKeyCode;
			} else {
				const wchar_t ws[] = {event->uChar.UnicodeChar, L'\0'};
				
				if (WideCharToMultiByte(CP_UTF8, 0, ws, -1, obj->tmp, sizeof(obj->tmp), NULL, NULL) == 0) {
					return NULL;
				}
			}
		#else
			obj->tmp[0] = event->uChar.AsciiChar == '\0' ? event->wVirtualKeyCode : event->uChar.AsciiChar;
		#endif
	#else
		const int fd = fileno(stdin);
		
		if (fd == -1) {
			return NULL;
		}
		
		const ssize_t size = read(fd, obj->tmp, sizeof(obj->tmp));
		
		if (size == -1) {
			return NULL;
		}
	#endif
	
	for (size_t index = 0; index < (sizeof(keys) / sizeof(*keys)); index++) {
		const struct CIKey* const key = &keys[index];
		
		#if defined(_WIN32)
			if (key->code == (DWORD) *obj->tmp) {
				return key;
			}
		#else
			if (memcmp(key->code, obj->tmp, strlen(obj->tmp)) == 0) {
				return key;
			}
		#endif
	}
	
	return &KEYBOARD_KEY_UNKNOWN;
	
}

int cir_free(struct CIR* const obj) {
	/*
	Free the Console Input Reader struct.
	
	On Linux, this will reset the standard input attributes back to "sane mode".
	Under Windows, this will add back the ENABLE_PROCESSED_INPUT bit to the standard input flags.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#if defined(_WIN32)
		const HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
		
		if (handle == INVALID_HANDLE_VALUE) {
			return -1;
		}
		
		if (SetConsoleMode(handle, obj->attributes) == 0) {
			return -1;
		}
	#else
		const int fd = fileno(stdin);
		
		if (fd == -1) {
			return -1;
		}
		
		int actions = TCSAFLUSH;
		
		#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
			actions |= TCSASOFT;
		#endif
		
		if (tcsetattr(fd, actions, &obj->attributes) == -1) {
			return -1;
		}
	#endif
	
	memset(obj->tmp, '\0', sizeof(obj->tmp));
	
	return 0;
	
}
