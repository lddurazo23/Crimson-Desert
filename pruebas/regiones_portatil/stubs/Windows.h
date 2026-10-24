#pragma once
#include <cstdint>
#define __fastcall
#define NTAPI
#define __try try
#define __except(x) catch(...)
#define EXCEPTION_EXECUTE_HANDLER 1
using ULONGLONG=unsigned long long;
using LONG=int32_t;
using ULONG=uint32_t;
using HANDLE=void*;
using HMODULE=void*;
using PVOID=void*;
using PULONG=ULONG*;
using FARPROC=void(*)();
ULONGLONG GetTickCount64();
HMODULE GetModuleHandleW(const wchar_t*);
HANDLE GetCurrentThread();
FARPROC GetProcAddress(HMODULE,const char*);
