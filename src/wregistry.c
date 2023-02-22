#include <stdlib.h>

#include <windows.h>

char* wregistry_get(const HKEY handle, const char* const path, const char* const key) {
	
	#ifdef _UNICODE
		const int wpaths = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
		
		if (wpaths == 0) {
			return NULL;
		}
		
		wchar_t wpath[wpaths];
		
		if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpaths) == 0) {
			return NULL;
		}
		
		const int wkeys = MultiByteToWideChar(CP_UTF8, 0, key, -1, NULL, 0);
		
		if (wkeys == 0) {
			return NULL;
		}
		
		wchar_t wkey[wkeys];
		
		if (MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, wkeys) == 0) {
			return NULL;
		}
		
		DWORD size = 0;
		
		if (RegGetValueW(handle, wpath, wkey, RRF_RT_ANY, NULL, NULL, &size) == ERROR_SUCCESS) {
			wchar_t wvalue[((size_t) size) / sizeof(wchar_t)];
			
			if (RegGetValueW(handle, wpath, wkey, RRF_RT_ANY, NULL, wvalue, &size) != ERROR_SUCCESS) {
				return NULL;
			}
			
			const int values = WideCharToMultiByte(CP_UTF8, 0, wvalue, -1, NULL, 0, NULL, NULL);
			
			if (values == 0) {
				return NULL;
			}
			
			char* const value = malloc(values);
			
			if (value == NULL) {
				return NULL;
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, wvalue, -1, value, values, NULL, NULL) == 0) {
				free(value);
				return NULL;
			}
			
			return value;
		} else {
			HKEY nkey = 0;
			
			if (RegOpenKeyExW(handle, wpath, 0, KEY_READ | KEY_WOW64_64KEY, &nkey) != ERROR_SUCCESS) {
				return NULL;
			}
			
			DWORD size = 0;
			
			if (RegGetValueW(nkey, NULL, wkey, RRF_RT_ANY, NULL, NULL, &size) != ERROR_SUCCESS) {
				RegCloseKey(nkey);
				return NULL;
			}
			
			wchar_t wvalue[((size_t) size) / sizeof(wchar_t)];
			
			if (RegGetValueW(nkey, NULL, wkey, RRF_RT_ANY, NULL, wvalue, &size) != ERROR_SUCCESS) {
				RegCloseKey(nkey);
				return NULL;
			}
			
			if (RegCloseKey(nkey) != ERROR_SUCCESS) {
				return NULL;
			}
			
			const int values = WideCharToMultiByte(CP_UTF8, 0, wvalue, -1, NULL, 0, NULL, NULL);
			
			if (values == 0) {
				return NULL;
			}
			
			char* const value = malloc(values);
			
			if (value == NULL) {
				return NULL;
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, wvalue, -1, value, values, NULL, NULL) == 0) {
				free(value);
				return NULL;
			}
			
			return value;
		}
	#else
		DWORD size = 0;
		
		if (RegGetValueA(handle, path, key, RRF_RT_ANY, NULL, NULL, &size) == ERROR_SUCCESS) {
			char* const value = malloc((size_t) size);
			
			if (value == NULL) {
				return NULL;
			}
			
			if (RegGetValueA(handle, path, key, RRF_RT_ANY, NULL, value, &size) != ERROR_SUCCESS) {
				free(value);
				return NULL;
			}
			
			return value;
		} else {
			HKEY nkey = 0;
			
			if (RegOpenKeyExA(handle, path, 0, KEY_READ | KEY_WOW64_64KEY, &nkey) != ERROR_SUCCESS) {
				return NULL;
			}
			
			DWORD size = 0;
			
			if (RegGetValueA(nkey, NULL, key, RRF_RT_ANY, NULL, NULL, &size) != ERROR_SUCCESS) {
				RegCloseKey(nkey);
				return NULL;
			}
			
			char* const value = malloc((size_t) size);
			
			if (value == NULL) {
				return NULL;
			}
			
			if (RegGetValueA(nkey, NULL, key, RRF_RT_ANY, NULL, value, &size) != ERROR_SUCCESS) {
				RegCloseKey(nkey);
				free(value);
				return NULL;
			}
			
			if (RegCloseKey(nkey) != ERROR_SUCCESS) {
				return NULL;
			}
			
			return value;
		}
	#endif
		
	return NULL;
	
}

int wregistry_put(const HKEY handle, const char* const path, const char* const key, const char* const value) {
	
	#ifdef _UNICODE
		const int wpaths = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
		
		if (wpaths == 0) {
			return -1;
		}
		
		wchar_t wpath[wpaths];
		
		if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpaths) == 0) {
			return -1;
		}
		
		const int wkeys = MultiByteToWideChar(CP_UTF8, 0, key, -1, NULL, 0);
		
		if (wkeys == 0) {
			return -1;
		}
		
		wchar_t wkey[wkeys];
		
		if (MultiByteToWideChar(CP_UTF8, 0, key, -1, wkey, wkeys) == 0) {
			return -1;
		}
		
		const int wvalues = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
		
		if (wvalues == 0) {
			return -1;
		}
		
		wchar_t wvalue[wvalues];
		
		if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wvalues) == 0) {
			return -1;
		}
		
		const LSTATUS status = RegSetKeyValueW(handle, wpath, wkey, REG_SZ, wvalue, sizeof(wvalue));
	#else
		const LSTATUS status = RegSetKeyValueA(handle, path, key, REG_SZ, value, strlen(value) + 1);
	#endif
	
	return (status == ERROR_SUCCESS) ? 0 : -1;
	
}
