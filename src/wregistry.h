#include <windows.h>

char* wregistry_get(const HKEY handle, const char* const path, const char* const key);
int wregistry_put(const HKEY handle, const char* const path, const char* const key, const char* const value);

#pragma once
