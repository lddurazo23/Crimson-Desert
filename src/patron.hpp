#pragma once
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace ds::patron {
inline int hexadecimal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
// Admite bytes hexadecimales y comodines completos; rechaza nibbles y firmas débiles.
inline bool analizar(const char* texto, std::vector<int>& bytes) {
    bytes.clear();
    if (!texto) return false;
    std::string_view t(texto);
    size_t i = 0, exactos = 0;
    while (i < t.size()) {
        while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) ++i;
        if (i == t.size()) break;
        size_t inicio = i;
        while (i < t.size() && !std::isspace(static_cast<unsigned char>(t[i]))) ++i;
        const auto token = t.substr(inicio, i - inicio);
        if (token == "?" || token == "??") bytes.push_back(-1);
        else if (token.size() == 2 && hexadecimal(token[0]) >= 0 && hexadecimal(token[1]) >= 0) {
            bytes.push_back(16 * hexadecimal(token[0]) + hexadecimal(token[1])); ++exactos;
        } else { bytes.clear(); return false; }
        if (bytes.size() > 1024) { bytes.clear(); return false; }
    }
    if (bytes.size() < 4 || exactos < 4) { bytes.clear(); return false; }
    return true;
}
struct Resultado {
    bool rangoValido = false;
    // Se satura en dos: dos o más coincidencias significan una firma ambigua.
    size_t cantidad = 0;
    size_t primera = 0;
};
inline bool rangoSinDesborde(uintptr_t inicio, size_t largo) {
    return inicio != 0 && largo != 0 && inicio <= std::numeric_limits<uintptr_t>::max() - (largo - 1);
}
inline Resultado buscar(const unsigned char* datos, size_t largo, const int* bytes, size_t n) {
    Resultado resultado{};
    if (!n || n > std::numeric_limits<size_t>::max() / sizeof(int) ||
        !rangoSinDesborde(reinterpret_cast<uintptr_t>(bytes), n * sizeof(int)) ||
        !rangoSinDesborde(reinterpret_cast<uintptr_t>(datos), largo)) return resultado;
    resultado.rangoValido = true;
    if (n > largo) return resultado;
    for (size_t k = 0; k < n; ++k) {
        if (bytes[k] < -1 || bytes[k] > 255) { resultado.rangoValido = false; return resultado; }
    }
    for (size_t i = 0; i <= largo - n; ++i) {
        size_t k = 0;
        for (; k < n; ++k)
            if (bytes[k] >= 0 && datos[i + k] != bytes[k]) break;
        if (k == n) {
            if (!resultado.cantidad) resultado.primera = i;
            if (++resultado.cantidad == 2) return resultado;
        }
    }
    return resultado;
}
}
