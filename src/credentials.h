struct Credentials {
	char* username;
	char* access_token;
	char* directory;
	char* cookie_jar;
};

void credentials_free(struct Credentials* obj);

#pragma once
