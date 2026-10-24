#ifdef NDEBUG
#undef NDEBUG
#endif
#include "patron.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main() {
    using namespace ds::patron;
    std::vector<int> p;
    assert(analizar("aa BB 00 ff ? ??", p));
    assert(p == std::vector<int>({0xAA, 0xBB, 0, 0xFF, -1, -1}));
    for (const char* invalido : {"", "   ", "?? ?? ?? ??", "48 8B ?? 00", "4? 8B 05 00", "GG 01 02 03", "0x48 01 02 03", "4 01 02 03", "AAA 01 02 03"}) {
        assert(!analizar(invalido, p));
        assert(p.empty());
    }
    assert(!analizar(nullptr, p));
    std::string enorme;
    for (size_t i = 0; i < 1025; ++i) enorme += "AA ";
    assert(!analizar(enorme.c_str(), p));
    assert(p.empty());

    assert(analizar("AA BB ?? DD", p) == false); // Solo tres bytes concretos.
    assert(analizar("AA BB ?? DD EE", p));
    const std::array<unsigned char, 14> datos{0xAA,0xBB,0,0xCC,0xEE,0xAA,0xBB,9,0xDD,0xEE,0,0,0,0};
    auto r = buscar(datos.data(), datos.size(), p.data(), p.size());
    assert(r.rangoValido && r.cantidad == 1 && r.primera == 5); // El primer prefijo es falso.
    const std::array<unsigned char, 9> borde{0xAA,0xBB,0,0xCC,0xEE,0xAA,0xBB,9,0xDD};
    r = buscar(borde.data(), borde.size(), p.data(), p.size());
    assert(r.rangoValido && r.cantidad == 0); // Nunca lee el byte fuera del límite indicado.
    r = buscar(datos.data() + 5, 5, p.data(), p.size());
    assert(r.rangoValido && r.cantidad == 1 && r.primera == 0); // Última posición permitida.
    r = buscar(datos.data(), 2, p.data(), p.size());
    assert(r.rangoValido && r.cantidad == 0);

    const std::array<unsigned char, 10> doble{0xAA,0xBB,1,0xDD,0xEE,0xAA,0xBB,2,0xDD,0xEE};
    r = buscar(doble.data(), doble.size(), p.data(), p.size());
    assert(r.rangoValido && r.cantidad == 2 && r.primera == 0);
    const std::array<int, 2> solapado{0xAA,0xAA};
    const std::array<unsigned char, 3> repetido{0xAA,0xAA,0xAA};
    r = buscar(repetido.data(), repetido.size(), solapado.data(), solapado.size());
    assert(r.rangoValido && r.cantidad == 2); // Detecta coincidencias que se solapan.
    assert(!buscar(nullptr, 5, p.data(), p.size()).rangoValido);
    assert(!buscar(datos.data(), 0, p.data(), p.size()).rangoValido);
    assert(!buscar(datos.data(), datos.size(), nullptr, 1).rangoValido);
    assert(!buscar(datos.data(), datos.size(), p.data(), 0).rangoValido);
    assert(!buscar(datos.data(), datos.size(), p.data(), std::numeric_limits<size_t>::max()).rangoValido);
    const auto finalInvalido = reinterpret_cast<const unsigned char*>(std::numeric_limits<uintptr_t>::max() - 1);
    assert(!buscar(finalInvalido, 5, p.data(), p.size()).rangoValido);
    assert(!rangoSinDesborde(std::numeric_limits<uintptr_t>::max(), 2));
    assert(rangoSinDesborde(std::numeric_limits<uintptr_t>::max(), 1));
    const std::array<int, 1> valorInvalido{256};
    assert(!buscar(datos.data(), datos.size(), valorInvalido.data(), 1).rangoValido);
    std::cout << "Pruebas de firmas: gramática, unicidad, límites y desbordes correctos.\n";
    return 0;
}
