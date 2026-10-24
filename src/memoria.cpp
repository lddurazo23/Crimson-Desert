#include "memoria.hpp"
#include "patron.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

namespace ds {
namespace {
bool rango(uintptr_t p, size_t n) {
    return p >= 0x10000 && n != 0 && p <= std::numeric_limits<uintptr_t>::max() - (n - 1);
}
// Estas rutinas no tienen objetos C++ con destructor dentro de __try.
bool copiarProtegido(void* destino, const void* origen, size_t n) {
    __try { std::memcpy(destino, origen, n); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool esEjecutable(DWORD p) {
    p &= 0xFF;
    return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ ||
           p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}
// El escaneo se limita a la imagen principal y conserva solo coincidencias únicas.
bool escanearRegion(uintptr_t base, size_t largo, const int* bytes, size_t n,
                    uintptr_t& encontrado, size_t& total) {
    if (largo < n) return true;
    __try {
        const auto* p = reinterpret_cast<const unsigned char*>(base);
        const auto resultado = patron::buscar(p, largo, bytes, n);
        if (!resultado.rangoValido || resultado.cantidad > 1 || total + resultado.cantidad > 1) return false;
        if (resultado.cantidad) encontrado = base + resultado.primera;
        total += resultado.cantidad;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}
bool leer(uintptr_t p, void* destino, size_t n) {
    return destino && rango(p, n) && copiarProtegido(destino, reinterpret_cast<void*>(p), n);
}
bool escribirDatos(uintptr_t p, const void* origen, size_t n) {
    if (!origen || !rango(p, n)) return false;
    const uintptr_t ultimo = p + n - 1;
    uintptr_t cursor = p;
    while (cursor <= ultimo) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &m, sizeof(m)) ||
            m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const DWORD permiso = m.Protect & 0xFF;
        if (permiso != PAGE_READWRITE && permiso != PAGE_WRITECOPY) return false;
        const uintptr_t fin = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
        if (fin <= cursor) return false;
        if (fin > ultimo) break;
        cursor = fin;
    }
    return copiarProtegido(reinterpret_cast<void*>(p), origen, n);
}
bool ejecutable(uintptr_t p) {
    MEMORY_BASIC_INFORMATION m{};
    return p && VirtualQuery(reinterpret_cast<void*>(p), &m, sizeof(m)) &&
        m.State == MEM_COMMIT && !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && esEjecutable(m.Protect);
}
uintptr_t buscarUnico(const char* texto) {
    std::vector<int> bytes;
    if (!patron::analizar(texto, bytes)) return 0;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!leer(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
        dos.e_lfanew > 1024 * 1024 || !leer(base + dos.e_lfanew, nt) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage < 4096 ||
        nt.OptionalHeader.SizeOfImage > 1024u * 1024u * 1024u) return 0;
    const uintptr_t fin = base + nt.OptionalHeader.SizeOfImage;
    uintptr_t cursor = base, encontrado = 0;
    size_t coincidencias = 0;
    while (cursor < fin) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &m, sizeof(m))) return 0;
        const uintptr_t siguiente = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
        if (siguiente <= cursor) return 0;
        if (m.State == MEM_COMMIT && !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && esEjecutable(m.Protect)) {
            const size_t largo = static_cast<size_t>(std::min(siguiente, fin) - cursor);
            if (!escanearRegion(cursor, largo, bytes.data(), bytes.size(), encontrado, coincidencias)) return 0;
        }
        cursor = siguiente;
    }
    return coincidencias == 1 ? encontrado : 0;
}
uintptr_t resolverRip(uintptr_t instruccion, uint32_t desplazamiento, uint32_t longitud) {
    int32_t relativo = 0;
    if (!instruccion || desplazamiento > longitud || longitud - desplazamiento < sizeof(relativo) ||
        instruccion > std::numeric_limits<uintptr_t>::max() - longitud ||
        !leer(instruccion + desplazamiento, relativo)) return 0;
    const auto origen = static_cast<int64_t>(instruccion + longitud);
    if (origen <= 0 || (relativo > 0 && origen > std::numeric_limits<int64_t>::max() - relativo)) return 0;
    const int64_t destino = origen + relativo;
    return destino >= 0x10000 ? static_cast<uintptr_t>(destino) : 0;
}
}

namespace ds {
bool testigoEnFuncion(uintptr_t funcion,const char* firmaTestigo) {
    std::vector<int> patronTestigo;
    if(!funcion || !patron::analizar(firmaTestigo,patronTestigo)) return false;
    const auto testigo=buscarUnico(firmaTestigo);
    if(!testigo) return false;
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if(!leer(base,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<=0 || dos.e_lfanew>1024*1024 ||
       !leer(base+static_cast<uintptr_t>(dos.e_lfanew),nt) || nt.Signature!=IMAGE_NT_SIGNATURE ||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    const auto resolver=reinterpret_cast<decltype(&RtlLookupFunctionEntry)>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlLookupFunctionEntry"));
    if(!resolver) return false;
    DWORD64 imagen=0;
    const auto registro=resolver(static_cast<DWORD64>(funcion),&imagen,nullptr);
    RUNTIME_FUNCTION ambito{};
    if(imagen!=base || !registro || !leer(reinterpret_cast<uintptr_t>(registro),ambito) ||
       ambito.BeginAddress>=ambito.EndAddress || ambito.EndAddress>nt.OptionalHeader.SizeOfImage) return false;
    const auto inicio=base+ambito.BeginAddress,fin=base+ambito.EndAddress;
    return funcion>=inicio && funcion<fin && testigo>=inicio && testigo<fin && patronTestigo.size()<=fin-testigo;
}
}
