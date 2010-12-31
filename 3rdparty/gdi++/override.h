
#define _CRT_SECURE_NO_DEPRECATE
#define _WIN32_WINNT 0x501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define for if(0);else for

void CacheInit();
void CacheTerm();

#define HOOK_DEFINE(dll, rettype, name, argtype, vars) \
	extern "C" rettype (WINAPI *ORIG_##name) argtype; \
	rettype WINAPI IMPL_##name argtype;

#include "hooklist.h"
#undef HOOK_DEFINE

//EOF
