#include <stdio.h>
#include <string.h>

#include "credentials.h"
#include "input.h"
#include "utils.h"
#include "errors.h"

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

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

char* get_username(char* const username) {
	
	while (1) {
		printf("> Insira seu usuário: ");
		
		if (fgets(username, MAX_INPUT_SIZE, stdin) != NULL && *username != '\n') {
			break;
		}
		
		fprintf(stderr, "- Usuário inválido ou não reconhecido!\r\n");
	}
	
	*strchr(username, '\n') = '\0';
	
	return username;
	
}

char* get_password(char* const password) {
	
	while (1) {
		printf("> Insira sua senha: ");
		
		if (fgets(password, MAX_INPUT_SIZE, stdin) != NULL && *password != '\n') {
			break;
		}
		
		fprintf(stderr, "- Senha inválida ou não reconhecida!\r\n");
	}
	
	*strchr(password, '\n') = '\0';
	
	return password;
	
}
/*
char username[MAX_INPUT_SIZE + 1] = {'\0'};

int ask_credentials(struct Credentials* const credentials) {
	
	char username[MAX_INPUT_SIZE + 1] = {'\0'};
	char password[MAX_INPUT_SIZE + 1] = {'\0'};
	
	while (1) {
		printf("> Insira seu usuário: ");
		
		if (fgets(username, sizeof(username), stdin) != NULL && *username != '\n') {
			break;
		}
		
		fprintf(stderr, "- Usuário inválido ou não reconhecido!\r\n");
	}
	
	*strchr(username, '\n') = '\0';
	
	while (1) {
		printf("> Insira sua senha: ");
		
		if (fgets(password, sizeof(password), stdin) != NULL && *password != '\n') {
			break;
		}
		
		fprintf(stderr, "- Senha inválida ou não reconhecida!\r\n");
	}
	
	*strchr(password, '\n') = '\0';
	
	const int code = authorize(username, password, obj);
	
	if (code != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível realizar a autenticação: %s\r\n", strurr(code));
		return 0;
	}
	
	credentials->username = malloc(strlen(username) + 1);
	
	if (credentials->username == NULL) {
		
	}
	
	strcpy(credentials->username, username);
	
	printf("+ Usuário autenticado com sucesso!\r\n");
	
	return 1;
	
}
*/