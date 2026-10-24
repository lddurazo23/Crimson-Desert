#pragma once
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <mutex>
#define __fastcall
#define __cdecl
#define WINAPI
#define __declspec(x)
#define __try try
#define __except(x) catch (...)
#define EXCEPTION_EXECUTE_HANDLER 1
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#define GET_MODULE_HANDLE_EX_FLAG_PIN 1
#define INVALID_HANDLE_VALUE reinterpret_cast<void*>(~uintptr_t(0))
#define GENERIC_READ 0x80000000u
#define FILE_SHARE_READ 1
#define FILE_SHARE_DELETE 4
#define OPEN_EXISTING 3
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define IMAGE_DOS_SIGNATURE 0x5a4d
#define IMAGE_NT_SIGNATURE 0x4550
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b
#define MAX_PATH 260
using DWORD = uint32_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using ULONG = uint32_t;
using ULONG_PTR = uintptr_t;
using ULONGLONG = uint64_t;
using BOOL = int;
using HANDLE = void*;
using HMODULE = void*;
using LPCWSTR = const wchar_t*;
using LPCVOID = const void*;
using LPVOID = void*;
using PUCHAR = unsigned char*;
struct SRWLOCK { std::mutex mutex; };
#define SRWLOCK_INIT {}
inline void AcquireSRWLockExclusive(SRWLOCK* valor) { valor->mutex.lock(); }
inline bool TryAcquireSRWLockExclusive(SRWLOCK* valor) { return valor->mutex.try_lock(); }
inline void ReleaseSRWLockExclusive(SRWLOCK* valor) { valor->mutex.unlock(); }
inline void AcquireSRWLockShared(SRWLOCK* valor) { valor->mutex.lock(); }
inline void ReleaseSRWLockShared(SRWLOCK* valor) { valor->mutex.unlock(); }
struct IMAGE_DOS_HEADER { WORD e_magic{}; unsigned char reservado[58]{}; int32_t e_lfanew{}; };
struct IMAGE_FILE_HEADER { WORD Machine{}; WORD NumberOfSections{}; DWORD TimeDateStamp{}; DWORD PointerToSymbolTable{}; DWORD NumberOfSymbols{}; WORD SizeOfOptionalHeader{}; WORD Characteristics{}; };
struct IMAGE_OPTIONAL_HEADER64 { WORD Magic{}; unsigned char reservado[54]{}; DWORD SizeOfImage{}; };
struct IMAGE_NT_HEADERS64 { DWORD Signature{}; IMAGE_FILE_HEADER FileHeader{}; IMAGE_OPTIONAL_HEADER64 OptionalHeader{}; };
extern uint64_t tiempoSimulado;
extern uintptr_t baseSimulada;
inline uint64_t GetTickCount64() { return tiempoSimulado; }
inline HMODULE GetModuleHandleW(LPCWSTR) { return reinterpret_cast<HMODULE>(baseSimulada); }
inline BOOL GetModuleHandleExW(DWORD, LPCWSTR, HMODULE* destino) { *destino = reinterpret_cast<HMODULE>(baseSimulada); return 1; }
inline DWORD GetModuleFileNameW(HMODULE, wchar_t* destino, DWORD longitud) { const wchar_t nombre[] = L"CrimsonDesert.exe"; if (longitud < sizeof(nombre) / sizeof(wchar_t)) return 0; std::wmemcpy(destino, nombre, sizeof(nombre) / sizeof(wchar_t)); return sizeof(nombre) / sizeof(wchar_t) - 1; }
inline HANDLE CreateFileW(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) { return INVALID_HANDLE_VALUE; }
inline BOOL ReadFile(HANDLE, void*, DWORD, DWORD*, void*) { return 0; }
inline BOOL CloseHandle(HANDLE) { return 1; }
