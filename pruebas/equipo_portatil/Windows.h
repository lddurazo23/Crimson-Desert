#pragma once
#include <cstddef>
#include <cstdint>
#define __fastcall
#define __try try
#define __except(x) catch (...)
#define EXCEPTION_EXECUTE_HANDLER 1
using DWORD = uint32_t;
constexpr DWORD MEM_COMMIT=0x1000, PAGE_GUARD=0x100, PAGE_NOACCESS=1;
constexpr DWORD PAGE_READWRITE=4, PAGE_WRITECOPY=8;
struct MEMORY_BASIC_INFORMATION { void* BaseAddress; size_t RegionSize; DWORD State, Protect; };
inline size_t VirtualQuery(void* p, MEMORY_BASIC_INFORMATION* m, size_t) {
    m->BaseAddress=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)&~uintptr_t(4095));
    m->RegionSize=4096; m->State=MEM_COMMIT; m->Protect=PAGE_READWRITE; return sizeof(*m);
}
