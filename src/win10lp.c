#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include <windows.h>
#include <io.h>

#include "utils.h"
#include "errors.h"

int get_unicode_value(
	const HKEY handle,
	const wchar_t* const path,
	const wchar_t* const key,
	wchar_t** value
) {
	
	DWORD size = 0;
	
	if (RegGetValueW(handle, path, key, RRF_RT_ANY, NULL, NULL, &size) == ERROR_SUCCESS) {
		wchar_t val[((size_t) size) * sizeof(wchar_t)];
		
		if (RegGetValueW(handle, path, key, RRF_RT_ANY, NULL, val, &size) != ERROR_SUCCESS) {
			return 0;
		}
		
		*value = malloc(sizeof(value));
		
		if (*value == NULL) {
			return 0;
		}
		
		wcscpy(*value, val);
	} else {
		HKEY nkey = 0;
		
		if (RegOpenKeyExW(handle, path, 0, KEY_READ | KEY_WOW64_64KEY, &nkey) != ERROR_SUCCESS) {
			return 0;
		}
		
		DWORD size = 0;
		
		if (RegGetValueW(nkey, NULL, key, RRF_RT_ANY, NULL, NULL, &size) != ERROR_SUCCESS) {
			RegCloseKey(nkey);
			return 0;
		}
		
		wchar_t val[((size_t) size) * sizeof(wchar_t)];
		
		if (RegGetValueW(nkey, NULL, key, RRF_RT_ANY, NULL, val, &size) != ERROR_SUCCESS) {
			RegCloseKey(nkey);
			return 0;
		}
		
		if (RegCloseKey(nkey) != ERROR_SUCCESS) {
			return 0;
		}
		
		*value = malloc(sizeof(value));
		
		if (*value == NULL) {
			return 0;
		}
		
		wcscpy(*value, val);
	}
	
	return 1;
	
}

int set_unicode_value(
	const HKEY handle,
	const wchar_t* const path,
	const wchar_t* const key,
	const wchar_t* const value
) {
	
	return RegSetKeyValueW(handle, path, key, REG_SZ, value, (wcslen(value) + 1) * sizeof(*value)) == ERROR_SUCCESS;

}

int wmain(void) {
	
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
	_setmode(_fileno(stdin), _O_WTEXT);
	
	const wchar_t* const path = L"System\\CurrentControlSet\\Control\\FileSystem";
	const wchar_t* const key = L"LongPathsEnabled";
	
	wchar_t* value = NULL;
	
	if (!is_administrator()) {
		fwprintf(stderr, L"- Você precisa executar este programa com privilégios administrativos!\r\n");
		return EXIT_FAILURE;
	}
	
	const int exists = get_unicode_value(HKEY_LOCAL_MACHINE, path, key, &value);
	
	const struct SystemError error = get_system_error();
	
	if (exists && wcscmp(value, L"dword:00000001") == 0) {
		fwprintf(stderr, L"- Seu sistema já foi configurado para aceitar caminhos de arquivo longos!\r\n");
		return EXIT_FAILURE;
	} else if (error.code != 0) {
		fwprintf(stderr, L"- Ocorreu uma falha inesperada ao tentar consultar as chaves de registro do Windows: %s\r\n", error.message);	
		return EXIT_FAILURE;
	} else {
		const int result = set_unicode_value(HKEY_LOCAL_MACHINE, path, key, L"dword:00000001");
		
		if (result) {
			wprintf(L"+ Seu sistema foi configurado para aceitar caminhos de arquivo longos\r\n");
		} else {
			const struct SystemError error = get_system_error();
			
			fwprintf(stderr, L"- Ocorreu uma falha inesperada ao tentar modificar as chaves de registro do Windows: %s\r\n", error.message);
			return EXIT_FAILURE;
		}
	}
	
	return EXIT_SUCCESS;
	
}
