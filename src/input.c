#if defined(_WIN32) && defined(_UNICODE)
	#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>

#include "credentials.h"
#include "input.h"
#include "utils.h"
#include "errors.h"

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

char* input(const char* const prompt, char* const answer) {
	
	while (1) {
		printf("%s", prompt);
		
		#if defined(_WIN32) && defined(_UNICODE)
			wchar_t wanswer[MAX_INPUT_SIZE];
			DWORD count = 0;
			
			const BOOL result = ReadConsoleW(
				GetStdHandle(STD_INPUT_HANDLE),
				wanswer,
				sizeof(wanswer) / sizeof(*wanswer),
				&count,
				NULL
			);
			
			if (result && *wanswer != L'\r') {
				WideCharToMultiByte(CP_UTF8, 0, wanswer, -1, answer, MAX_INPUT_SIZE, NULL, NULL);
				break;
			}
		#else
			if (fgets(answer, MAX_INPUT_SIZE, stdin) != NULL && *answer != '\n') {
				break;
			}
		#endif
		
		fprintf(stderr, "- O valor inserido é inválido ou não reconhecido!\r\n");
	}
	
	#if defined(_WIN32) && defined(_UNICODE)
		*strchr(answer, '\r') = '\0';
	#else
		*strchr(answer, '\n') = '\0';
	#endif
	
	return answer;
	
}

int input_integer(const int min, const int max) {
	
	char answer[MAX_INPUT_SIZE];
	int value = 0;
	
	while (1) {
		input("> Digite sua escolha: ", answer);
		
		if (isnumeric(answer)) {
			value = atoi(answer);
			
			if (value >= min && value <= max) {
				break;
			}
		}
		
		fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
	}
	
	return value;
	
}
