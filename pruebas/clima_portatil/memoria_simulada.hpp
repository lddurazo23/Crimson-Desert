#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace simulacion_clima {
struct Region {
    uintptr_t inicio;
    size_t longitud;
};
inline std::vector<Region> regiones;
inline uintptr_t escrituraDenegada = 0;
inline size_t escrituras = 0;

template<size_t N> void registrar(std::array<unsigned char, N>& memoria) {
    regiones.push_back({reinterpret_cast<uintptr_t>(memoria.data()), memoria.size()});
}
inline bool accesible(uintptr_t direccion, size_t longitud) {
    for (const auto& region : regiones) {
        if (direccion >= region.inicio && longitud <= region.longitud &&
            direccion - region.inicio <= region.longitud - longitud) return true;
    }
    return false;
}
template<class T, size_t N> void poner(std::array<unsigned char, N>& memoria,
                                     size_t desplazamiento, T valor) {
    assert(desplazamiento + sizeof(T) <= memoria.size());
    std::memcpy(memoria.data() + desplazamiento, &valor, sizeof(T));
}
template<class T, size_t N> T sacar(const std::array<unsigned char, N>& memoria,
                                  size_t desplazamiento) {
    assert(desplazamiento + sizeof(T) <= memoria.size());
    T valor{};
    std::memcpy(&valor, memoria.data() + desplazamiento, sizeof(T));
    return valor;
}
}
