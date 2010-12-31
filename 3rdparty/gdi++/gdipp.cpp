/*
    Copyright (C) 2010 Nikita Nemkin <nikita@nemkin.ru>

    This file is part of PerpetuumPlanner.

    PerpetuumPlanner is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PerpetuumPlanner is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PerpetuumPlanner. If not, see <http://www.gnu.org/licenses/>.
*/

#include "override.h"
#include "gdipp.h"


static DWORD HookIATByName(const char *funcName, DWORD newFunc)
{
	IMAGE_DOS_HEADER *pDosHeader = (IMAGE_DOS_HEADER *) GetModuleHandle(0);
	if (IsBadReadPtr(pDosHeader, sizeof(IMAGE_DOS_HEADER)) || pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	IMAGE_NT_HEADERS *pNtHeaders = (IMAGE_NT_HEADERS *) ((BYTE *) pDosHeader + pDosHeader->e_lfanew);
	if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}
	IMAGE_IMPORT_DESCRIPTOR* pImportDesc = (IMAGE_IMPORT_DESCRIPTOR *)
		((BYTE *) pDosHeader + pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	while (pImportDesc->FirstThunk) {
		// const char *pszDllName = (const char *) ((BYTE *) pDosHeader + pImportDesc->Name);
		IMAGE_THUNK_DATA* pOrigThunk = (IMAGE_THUNK_DATA*) ((BYTE *) pDosHeader + pImportDesc->OriginalFirstThunk);
		IMAGE_THUNK_DATA* pThunk = (IMAGE_THUNK_DATA*) ((BYTE *) pDosHeader + pImportDesc->FirstThunk);

		while (pOrigThunk->u1.Function) {
			if (!(pOrigThunk->u1.AddressOfData & 0x80000000)) { // not ordinal
				const char* pszFunName = (const char *) ((BYTE *) pDosHeader + (DWORD) pOrigThunk->u1.AddressOfData + 2);

				if (strcmp(pszFunName, funcName) == 0) {
					DWORD ret = pThunk->u1.Function;
					DWORD dwOldProtect;
					if (VirtualProtect((void *) &pThunk->u1.Function, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
						pThunk->u1.Function = newFunc;
						VirtualProtect((void *) &pThunk->u1.Function, sizeof(DWORD), dwOldProtect, &dwOldProtect);
					}
					return ret;
				}
			}
			++pThunk;
			++pOrigThunk;
		}
		++pImportDesc;
	}
	return 0;
}


#define HOOK_DEFINE(dll, rettype, name, argtype, vars) \
	rettype (WINAPI *ORIG_##name) argtype; \
	extern "C" __declspec(dllimport) rettype WINAPI name argtype;
#include "hooklist.h"
#undef HOOK_DEFINE


Gdipp::Gdipp()
{
	CacheInit();

#define HOOK_DEFINE(dll, rettype, name, argtype, vars) \
	reinterpret_cast<DWORD &>(ORIG_##name) = HookIATByName(#name, (DWORD) &IMPL_##name);

#include "hooklist.h"
#undef HOOK_DEFINE
}

Gdipp::~Gdipp()
{
	CacheTerm();
}

extern UINT g_ForceClearType;

bool Gdipp::enableClearType()
{
	return g_ForceClearType;
}

void Gdipp::setEnableClearType(bool enable)
{
	g_ForceClearType = enable;
}
