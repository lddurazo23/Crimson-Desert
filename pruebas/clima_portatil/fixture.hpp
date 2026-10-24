#pragma once
#include "memoria_simulada.hpp"
#include "datos_juego.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace simulacion_clima {
namespace d = ds::datos::clima;
inline std::array<unsigned char, 0x80> servicio{}, intermediario{};
inline std::array<unsigned char, 0x100> administrador{};
inline std::array<unsigned char, 0x1000> mundo{};
inline std::array<unsigned char, 0x100> recurso{}, otroRecurso{};
inline std::array<unsigned char, 0x240> constantes{}, otrasConstantes{};
inline std::array<unsigned char, 0x180> atmosfera{}, otraAtmosfera{};
inline std::array<unsigned char, 0x300> campo{};
inline std::array<unsigned char, 0x60> buffers{}, bufferActivo{};
inline uintptr_t punteroServicio{};
inline constexpr uintptr_t baseFicticia = 0x10000000;

template<size_t N> uintptr_t direccion(std::array<unsigned char, N>& memoria) {
    return reinterpret_cast<uintptr_t>(memoria.data());
}
template<size_t N> void inicializarRecurso(std::array<unsigned char, N>& resultado,
                                         std::array<unsigned char, 0x240>& clima,
                                         std::array<unsigned char, 0x180>& aire) {
    poner(resultado, 0, baseFicticia + d::tablaRecurso);
    poner(resultado, d::recursoClima, direccion(clima) + d::ajusteInterfaz);
    poner(resultado, d::recursoAtmosfera, direccion(aire) + d::ajusteInterfaz);
    poner(clima, 0, baseFicticia + d::tablaClima);
    poner(aire, 0, baseFicticia + d::tablaAtmosfera);
    poner(clima, d::ajusteInterfaz - d::hijoPadre, direccion(resultado));
    poner(aire, d::ajusteInterfaz - d::hijoPadre, direccion(resultado));
    poner(clima, d::ajusteInterfaz + d::climaPrecipitacion, .2f);
    poner(clima, d::ajusteInterfaz + d::climaNubosidad, .3f);
    poner(clima, d::ajusteInterfaz + d::climaHumedad, .4f);
    poner(clima, d::ajusteInterfaz + d::climaViento, 10.f);
    poner(clima, d::ajusteInterfaz + d::climaFase, 1.f);
    poner(aire, d::ajusteInterfaz + d::atmosferaAerosol, .1f);
    poner(aire, d::ajusteInterfaz + d::atmosferaNiebla, 2.f);
    poner(aire, d::ajusteInterfaz + d::atmosferaNubes, .3f);
}
inline void preparar() {
    regiones.clear();
    for (auto* memoria : {&servicio, &intermediario}) {
        memoria->fill(0); registrar(*memoria);
    }
    administrador.fill(0); registrar(administrador);
    mundo.fill(0); registrar(mundo);
    recurso.fill(0); registrar(recurso);
    otroRecurso.fill(0); registrar(otroRecurso);
    constantes.fill(0); registrar(constantes);
    otrasConstantes.fill(0); registrar(otrasConstantes);
    atmosfera.fill(0); registrar(atmosfera);
    otraAtmosfera.fill(0); registrar(otraAtmosfera);
    campo.fill(0); registrar(campo);
    buffers.fill(0); registrar(buffers);
    bufferActivo.fill(0); registrar(bufferActivo);
    punteroServicio = direccion(servicio);
    poner(servicio, d::servicioIntermedio, direccion(intermediario));
    poner(intermediario, d::intermedioAdministrador, direccion(administrador));
    poner(administrador, 0, baseFicticia + d::tablaAdministrador);
    poner(administrador, d::administradorMundo, direccion(mundo));
    poner(mundo, 0, baseFicticia + d::tablaMundo);
    poner(mundo, d::mundoAdministrador, direccion(administrador));
    poner(administrador, d::administradorResultado, direccion(recurso) + d::ajusteInterfaz);
    inicializarRecurso(recurso, constantes, atmosfera);
    inicializarRecurso(otroRecurso, otrasConstantes, otraAtmosfera);
    poner(campo, 0, baseFicticia + d::tablaCampo);
    poner(campo, d::campoBuffers, direccion(buffers));
    poner(buffers, d::buffersFase, uint8_t(5));
    poner(buffers, d::buffersEntradas + 5 * sizeof(uintptr_t), direccion(bufferActivo));
    poner(bufferActivo, d::bufferActivo, uint8_t(1));
    poner(campo, d::campoMundo, uint32_t(7));
    poner(campo, d::campoEtapa, uint16_t(42));
    poner(campo, d::campoMascara, uint32_t(32));
    escrituraDenegada = 0; escrituras = 0;
}
inline void manualNativo(void* objeto, const uint8_t* valor) {
    std::memcpy(static_cast<unsigned char*>(objeto) + d::manual, valor, sizeof(*valor));
}
template<uintptr_t desplazamiento> void escalarNativo(void* objeto, const float* valor) {
    std::memcpy(static_cast<unsigned char*>(objeto) + d::ajusteInterfaz + desplazamiento,
                valor, sizeof(*valor));
}
inline void derivarNativo(void* objeto) {
    uintptr_t clima{};
    std::memcpy(&clima, static_cast<unsigned char*>(objeto) + d::recursoClima, sizeof(clima));
    float precipitacion{}, fase{};
    std::memcpy(&precipitacion, reinterpret_cast<void*>(clima + d::climaPrecipitacion), sizeof(float));
    std::memcpy(&fase, reinterpret_cast<void*>(clima + d::climaFase), sizeof(float));
    const float lluvia = fase > 0 ? precipitacion : 0.f;
    const float nieve = fase > 0 ? 0.f : precipitacion;
    std::memcpy(reinterpret_cast<void*>(clima + d::climaLluvia), &lluvia, sizeof(float));
    std::memcpy(reinterpret_cast<void*>(clima + d::climaNieve), &nieve, sizeof(float));
}
}

namespace ds {
inline bool leer(uintptr_t direccion, void* destino, size_t cantidad) {
    using namespace simulacion_clima;
    if (direccion == baseFicticia + d::servicio && cantidad == sizeof(punteroServicio)) {
        std::memcpy(destino, &punteroServicio, cantidad); return true;
    }
    if (!accesible(direccion, cantidad)) return false;
    std::memcpy(destino, reinterpret_cast<const void*>(direccion), cantidad); return true;
}
inline bool escribirDatos(uintptr_t direccion, const void* origen, size_t cantidad) {
    using namespace simulacion_clima;
    if (!accesible(direccion, cantidad) || direccion == escrituraDenegada) return false;
    std::memcpy(reinterpret_cast<void*>(direccion), origen, cantidad); ++escrituras; return true;
}
inline bool ejecutable(uintptr_t) { return true; }
inline uintptr_t buscarUnico(const char*) { return 0; }
inline uintptr_t resolverRip(uintptr_t, uint32_t, uint32_t) { return 0; }
}
