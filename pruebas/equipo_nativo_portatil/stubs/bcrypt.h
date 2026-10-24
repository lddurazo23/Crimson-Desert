#pragma once
#include "Windows.h"
using BCRYPT_ALG_HANDLE = void*;
using BCRYPT_HASH_HANDLE = void*;
#define BCRYPT_SHA256_ALGORITHM L"SHA256"
inline int BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE*, LPCWSTR, LPCWSTR, ULONG) { return -1; }
inline int BCryptCreateHash(BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG) { return -1; }
inline int BCryptHashData(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG) { return -1; }
inline int BCryptFinishHash(BCRYPT_HASH_HANDLE, PUCHAR, ULONG, ULONG) { return -1; }
inline int BCryptDestroyHash(BCRYPT_HASH_HANDLE) { return 0; }
inline int BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE, ULONG) { return 0; }
