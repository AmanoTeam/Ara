#ifdef _WIN32
	#include <windows.h>
#else
	#include <stdio.h>
	#include <termios.h>
#endif

#include "terminal.h"

int erase_screen(void) {
	/*
	Erases the screen with the background colour and moves the cursor to home.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#ifdef _WIN32
		const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		
		if (handle == INVALID_HANDLE_VALUE) {
			return -1;
		}
		
		CONSOLE_SCREEN_BUFFER_INFO info = {0};
		
		if (GetConsoleScreenBufferInfo(handle, &info) == 0) {
			return -1;
		}
		
		const DWORD value = (DWORD) (info.dwSize.X * info.dwSize.Y);
		
		DWORD wrote = 0;
		COORD origin = {0};
		
		#ifdef _UNICODE
			const BOOL status = FillConsoleOutputCharacterW(handle, L' ', value, origin, &wrote);
		#else
			const BOOL status = FillConsoleOutputCharacterA(handle, ' ', value, origin, &wrote);
		#endif
		
		if (status == 0) {
			return -1;
		}
		
		if (FillConsoleOutputAttribute(handle, info.wAttributes, value, origin, &wrote) == 0) {
			return -1;
		}
		
		info.dwCursorPosition.X = 0;
		
		if (SetConsoleCursorPosition(handle, info.dwCursorPosition) == 0) {
			return -1;
		}
	#else
		if (printf("\033[2J") < 1) {
			return -1;
		}
	#endif
	
	return 0;
	
}

int erase_line(void) {
	/*
	Erases the entire current line.
	
	Returns (0) on success, (-1) on error.
	*/
	
	#ifdef _WIN32
		const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
		
		if (handle == INVALID_HANDLE_VALUE) {
			return -1;
		}
		
		CONSOLE_SCREEN_BUFFER_INFO info = {0};
		
		if (GetConsoleScreenBufferInfo(handle, &info) == 0) {
			return -1;
		}
		
		COORD origin = info.dwCursorPosition;
		origin.X = 0;
		
		if (SetConsoleCursorPosition(handle, origin) == 0) {
			return -1;
		}
		
		DWORD wrote = 0;
		DWORD value = (DWORD) (info.dwSize.X - origin.X);
		
		#ifdef _UNICODE
			const BOOL status = FillConsoleOutputCharacterW(handle, L' ', value, origin, &wrote);
		#else
			const BOOL status = FillConsoleOutputCharacterA(handle, ' ', value, origin, &wrote);
		#endif
		
		if (status == 0) {
			return -1;
		}
		
		if (FillConsoleOutputAttribute(handle, info.wAttributes, value, info.dwCursorPosition, &wrote) == 0) {
			return -1;
		}
	#else
		if (printf("\033[2K") < 1) {
			return -1;
		}
		
		if (printf("\033[1G") < 1) {
			return -1;
		}
	#endif
	
	return 0;
	
}
