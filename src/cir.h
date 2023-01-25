#include <stdlib.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <termios.h>
#endif

struct CIR {
#ifdef _WIN32
	DWORD attributes;
#else
	struct termios attributes;
#endif
	char tmp[16];
};

enum CIKeyType {
	KEY_PAGE_UP,
	KEY_PAGE_DOWN,
	KEY_ARROW_UP,
	KEY_ARROW_DOWN,
	KEY_ARROW_LEFT,
	KEY_ARROW_RIGHT,
	KEY_ENTER,
	KEY_HOME,
	KEY_END,
	KEY_DELETE,
	KEY_CTRL_C,
	KEY_CTRL_BACKSLASH,
	KEY_CTRL_D,
	KEY_ZERO,
	KEY_ONE,
	KEY_TWO,
	KEY_THREE,
	KEY_FOUR,
	KEY_FIVE,
	KEY_SIX,
	KEY_SEVEN,
	KEY_EIGHTH,
	KEY_NINE,
	KEY_HYPHEN,
	KEY_COMMA,
	KEY_SHIFT_S,
	KEY_S,
	KEY_SHIFT_Y,
	KEY_Y,
	KEY_SHIFT_N,
	KEY_N,
	KEY_EMPTY,
	KEY_UNKNOWN
};

struct CIKey {
	enum CIKeyType type;
	char* name;
#ifdef _WIN32
	WORD code;
#else
	char code[5];
#endif
};

static const struct CIKey KEYBOARD_KEY_EMPTY = {
	.type = KEY_EMPTY,
	.name = "Empty",
	#ifdef _WIN32
		.code = 0x0
	#else
		.code = {0x0}
	#endif
};

static const struct CIKey KEYBOARD_KEY_UNKNOWN = {
	.type = KEY_UNKNOWN,
	.name = "Unknown",
	#ifdef _WIN32
		.code = 0x0
	#else
		.code = {0x0}
	#endif
};

static const struct CIKey keys[] = {
	{
		.type = KEY_PAGE_UP,
		.name = "Page Up",
	#ifdef _WIN32
		.code = 0x21
	#else
		.code = {0x1b, 0x5b, 0x35, 0x7e}
	#endif
	},
	{
		.type = KEY_PAGE_DOWN,
		.name = "Page Down",
	#ifdef _WIN32
		.code = 0x22
	#else
		.code = {0x1b, 0x5b, 0x36, 0x7e}
	#endif
	},
	{
		.type = KEY_ARROW_UP,
		.name = "Arrow Up",
	#ifdef _WIN32
		.code = 0x26
	#else
		.code = {0x1b, 0x5b, 0x41}
	#endif
	},
	{
		.type = KEY_ARROW_DOWN,
		.name = "Arrow Down",
	#ifdef _WIN32
		.code = 0x28
	#else
		.code = {0x1b, 0x5b, 0x42}
	#endif
	},
	{
		.type = KEY_ARROW_LEFT,
		.name = "Arrow Left",
	#ifdef _WIN32
		.code = 0x25
	#else
		.code = {0x1b, 0x5b, 0x44}
	#endif
	},
	{
		.type = KEY_ARROW_RIGHT,
		.name = "Arrow Right",
	#ifdef _WIN32
		.code = 0x27
	#else
		.code = {0x1b, 0x5b, 0x43}
	#endif
	},
	{
		.type = KEY_ENTER,
		.name = "Enter",
	#ifdef _WIN32
		.code = 0xd
	#else
		.code = {0xd}
	#endif
	},
	{
		.type = KEY_HOME,
		.name = "Home",
	#ifdef _WIN32
		.code = 0x24
	#else
		.code = {0x1b, 0x5b, 0x48}
	#endif
	},
	{
		.type = KEY_END,
		.name = "End",
	#ifdef _WIN32
		.code = 0x23
	#else
		.code = {0x1b, 0x5b, 0x46}
	#endif
	},
	{
		.type = KEY_DELETE,
		.name = "Delete",
	#ifdef _WIN32
		.code = 0x8
	#else
		.code = {0x7f}
	#endif
	},
	{
		.type = KEY_CTRL_C,
		.name = "CTRL + C",
	#ifdef _WIN32
		.code = 0x999
	#else
		.code = {0x3}
	#endif
	},
	{
		.type = KEY_CTRL_BACKSLASH,
		.name = "CTRL + \\",
	#ifdef _WIN32
		.code = 0x999
	#else
		.code = {0x1c}
	#endif
	},
	{
		.type = KEY_CTRL_D,
		.name = "CTRL + D",
	#ifdef _WIN32
		.code = 0x999
	#else
		.code = {0x4}
	#endif
	},
	{
		.type = KEY_ZERO,
		.name = "0",
	#ifdef _WIN32
		.code = 0x30
	#else
		.code = {0x30}
	#endif
	},
	{
		.type = KEY_ONE,
		.name = "1",
	#ifdef _WIN32
		.code = 0x31
	#else
		.code = {0x31}
	#endif
	},
	{
		.type = KEY_TWO,
		.name = "2",
	#ifdef _WIN32
		.code = 0x32
	#else
		.code = {0x32}
	#endif
	},
	{
		.type = KEY_THREE,
		.name = "3",
	#ifdef _WIN32
		.code = 0x33
	#else
		.code = {0x33}
	#endif
	},
	{
		.type = KEY_FOUR,
		.name = "4",
	#ifdef _WIN32
		.code = 0x34
	#else
		.code = {0x34}
	#endif
	},
	{
		.type = KEY_FIVE,
		.name = "5",
	#ifdef _WIN32
		.code = 0x35
	#else
		.code = {0x35}
	#endif
	},
	{
		.type = KEY_SIX,
		.name = "6",
	#ifdef _WIN32
		.code = 0x36
	#else
		.code = {0x36}
	#endif
	},
	{
		.type = KEY_SEVEN,
		.name = "7",
	#ifdef _WIN32
		.code = 0x37
	#else
		.code = {0x37}
	#endif
	},
	{
		.type = KEY_EIGHTH,
		.name = "8",
	#ifdef _WIN32
		.code = 0x38
	#else
		.code = {0x38}
	#endif
	},
	{
		.type = KEY_NINE,
		.name = "9",
	#ifdef _WIN32
		.code = 0x39
	#else
		.code = {0x39}
	#endif
	},
	{
		.type = KEY_HYPHEN,
		.name = "-",
	#ifdef _WIN32
		.code = 0x2d
	#else
		.code = {0x2d}
	#endif
	},
	{
		.type = KEY_COMMA,
		.name = ",",
	#ifdef _WIN32
		.code = 0x2c
	#else
		.code = {0x2c}
	#endif
	},
	{
		.type = KEY_SHIFT_S,
		.name = "S",
	#ifdef _WIN32
		.code = 0x53
	#else
		.code = {0x53}
	#endif
	},
	{
		.type = KEY_S,
		.name = "s",
	#ifdef _WIN32
		.code = 0x73
	#else
		.code = {0x73}
	#endif
	},
	{
		.type = KEY_SHIFT_N,
		.name = "N",
	#ifdef _WIN32
		.code = 0x4e
	#else
		.code = {0x4e}
	#endif
	},
	{
		.type = KEY_N,
		.name = "n",
	#ifdef _WIN32
		.code = 0x6e
	#else
		.code = {0x6e}
	#endif
	},
	{
		.type = KEY_SHIFT_Y,
		.name = "Y",
	#ifdef _WIN32
		.code = 0x59
	#else
		.code = {0x59}
	#endif
	},
	{
		.type = KEY_Y,
		.name = "y",
	#ifdef _WIN32
		.code = 0x79
	#else
		.code = {0x79}
	#endif
	}
};

int cir_init(struct CIR* const obj);
const struct CIKey* cir_get(struct CIR* const obj);
int cir_free(struct CIR* const obj);

#pragma once
