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
		
		if (fgets(answer, MAX_INPUT_SIZE, stdin) != NULL && *answer != '\n') {
			break;
		}
		
		fprintf(stderr, "- O valor inserido é inválido ou não reconhecido!\r\n");
	}
	
	*strchr(answer, '\n') = '\0';
	
	return answer;
	
}

int input_integer(const int min, const int max) {
	
	char answer[MAX_INPUT_SIZE];
	int value = 0;
	
	while (1) {
		printf("> Digite sua escolha: ");
		
		if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
			*strchr(answer, '\n') = '\0';
			
			if (isnumeric(answer)) {
				value = atoi(answer);
				
				if (value >= min && value <= max) {
					break;
				}
			}
		}
		
		fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
	}
	
	return value;
	
}
