#ifdef __cplusplus
extern "C" {
#endif

#ifndef __HAIKU__
	int is_administrator(void);
#endif

char* get_configuration_directory(void);
char* get_temporary_directory(void);
char* get_home_directory(void);
char* find_exe(const char* const name);

#ifdef __cplusplus
}
#endif

#pragma once
