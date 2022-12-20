struct Credentials {
	char* username;
	char* access_token;
};

void credentials_free(struct Credentials* obj);

#pragma once
