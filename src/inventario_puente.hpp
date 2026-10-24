#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <vector>
#include "datos_juego.hpp"
#include "memoria.hpp"
#include "inventario.hpp"

namespace trinity {
struct State {
    bool invStackSize = false;
    int64_t invStackSizeVal = 0;
    bool invSlotSize = false;
    int invSlotSizeVal = 0;
    static const State& Get() { static const State estado; return estado; }
};
inline void informarMotor() {
    OutputDebugStringW(L"DesiertoSeguro: se rechazó una etapa del inventario; revisa el estado en el menú.\n");
}
namespace mem {
template<class T> inline bool Read64(uintptr_t p, T* v) {
    static_assert(sizeof(T) == 8); return v && ds::leer(p, *v);
}
inline bool Read32(uintptr_t p, uint32_t* v) { return v && ds::leer(p, *v); }
inline bool Read16(uintptr_t p, uint16_t* v) { return v && ds::leer(p, *v); }
inline bool Read8(uintptr_t p, uint8_t* v) { return v && ds::leer(p, *v); }
inline bool ReadPtr(uintptr_t p, uintptr_t* v) { return Read64(p, v); }
inline bool Write64(uintptr_t p, uint64_t v) { return ds::escribir(p, v); }
inline bool Write16(uintptr_t p, uint16_t v) { return ds::escribir(p, v); }
inline bool Write8(uintptr_t p, uint8_t v) { return ds::escribir(p, v); }
inline bool ReadCString(uintptr_t p, char* destino, size_t n) {
    if (!destino || n < 2 || !p) return false;
    destino[0] = 0;
    for (size_t i = 0; i < n - 1; ++i) {
        if (!ds::leer(p + i, destino[i])) { destino[0] = 0; return false; }
        if (!destino[i]) return true;
    }
    destino[n - 1] = 0;
    return true;
}
inline uintptr_t FindPattern(const char* texto) { return ds::buscarUnico(texto); }
inline uintptr_t ResolveRipAt(uintptr_t p, uint32_t n) {
    return n >= 4 ? ds::resolverRip(p, n - 4, n) : 0;
}
// Los anclajes cortos de tablas sólo se aceptan después de verificar el nombre
// exacto y el prólogo publicado. Dos funciones distintas válidas se rechazan.
inline uintptr_t FindPatternIf(const char* texto, bool(*predicado)(uintptr_t, void*), void* contexto) {
    if (!texto || !predicado) return 0;
    std::vector<int> patron;
    for (size_t i = 0; texto[i];) {
        while (texto[i] && std::isspace(static_cast<unsigned char>(texto[i]))) ++i;
        if (!texto[i]) break;
        if (texto[i] == '?') {
            patron.push_back(-1); ++i; if (texto[i] == '?') ++i;
        } else {
            unsigned v = 0;
            if (!texto[i + 1] || std::sscanf(texto + i, "%2x", &v) != 1) return 0;
            patron.push_back(static_cast<int>(v)); i += 2;
        }
        if (texto[i] && !std::isspace(static_cast<unsigned char>(texto[i]))) return 0;
        if (patron.size() > 1024) return 0;
    }
    if (patron.empty()) return 0;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if (!ds::leer(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
        dos.e_lfanew > 1024*1024 || !ds::leer(base + dos.e_lfanew, nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt.OptionalHeader.SizeOfImage > 1024u*1024u*1024u) return 0;
    const auto fin = base + nt.OptionalHeader.SizeOfImage;
    uintptr_t encontrado = 0;
    bool ambiguo = false;
    for (uintptr_t cursor = base; cursor < fin;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &m, sizeof(m))) return 0;
        const auto siguiente = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
        if (siguiente <= cursor) return 0;
        const DWORD permiso = m.Protect & 0xFF;
        if (m.State == MEM_COMMIT && !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            (permiso == PAGE_EXECUTE_READ || permiso == PAGE_EXECUTE_READWRITE || permiso == PAGE_EXECUTE_WRITECOPY)) {
            const size_t n = std::min(siguiente, fin) - cursor;
            std::vector<uint8_t> bytes(n);
            if (!ds::leer(cursor, bytes.data(), n)) return 0;
            for (size_t i = 0; n >= patron.size() && i <= n - patron.size(); ++i) {
                size_t j = 0;
                while (j < patron.size() && (patron[j] < 0 || patron[j] == bytes[i+j])) ++j;
                if (j == patron.size() && predicado(cursor+i, contexto)) {
                    if (encontrado && encontrado != cursor+i) ambiguo = true;
                    encontrado = cursor+i;
                }
            }
        }
        cursor = siguiente;
    }
    return ambiguo ? 0 : encontrado;
}
template<class Fn> bool InstallHook(const char*, const char* patron, const char*,
                                  Fn reemplazo, Fn* original, void** objetivo, size_t = 1) {
    *objetivo = nullptr; *original = nullptr;
    const auto p = FindPattern(patron);
    if (!p || !ds::ejecutable(p)) return false;
    void* direccion = reinterpret_cast<void*>(p);
    if (MH_CreateHook(direccion, reinterpret_cast<void*>(reemplazo), reinterpret_cast<void**>(original)) != MH_OK)
        return false;
    if (MH_EnableHook(direccion) != MH_OK) {
        MH_RemoveHook(direccion); *original = nullptr; return false;
    }
    *objetivo = direccion; return true;
}
inline void RemoveHook(void** p) {
    if (*p) { MH_DisableHook(*p); MH_RemoveHook(*p); *p = nullptr; }
}
}
}
