#include "fixture.hpp"
#include "MinHook.h"
#include <iostream>
#include <latch>
#include <string>
#include <thread>

uint64_t tiempoSimulado = 1000;
uintptr_t baseSimulada = simulacion_clima::baseFicticia;
void* direccionRetornoSimulada = nullptr;
unsigned hooksCreados = 0;
unsigned llamadasProductor = 0;
float deltaObservado = 0;
void productorSimulado(void*, float delta) { ++llamadasProductor; deltaObservado = delta; }
MH_STATUS MH_Initialize() { return MH_OK; }
MH_STATUS MH_CreateHook(void*, void*, void** original) { ++hooksCreados; *original = reinterpret_cast<void*>(&productorSimulado); return MH_OK; }
MH_STATUS MH_EnableHook(void*) { return MH_OK; }
MH_STATUS MH_DisableHook(void*) { return MH_OK; }
MH_STATUS MH_RemoveHook(void*) { return MH_OK; }

// Se incluye la implementación para someter a prueba sus mismos validadores y
// su mismo controlador. Solo los destinos nativos se sustituyen por fixtures.
#include "../../src/clima.cpp"

namespace s = simulacion_clima;
namespace d = ds::datos::clima;
using s::direccion;
using s::poner;
using s::sacar;

void reiniciar(bool manual = true) {
    s::preparar();
    ds::baseClima = s::baseFicticia;
    ds::conectado.store(true); ds::solicitado.store(false); ds::disponible.store(false);
    ds::fallo.store(false); ds::estado.store(0); ds::ultimoCliente.store(0);
    ds::testigo = {}; ds::identidadPropia = {}; ds::original = {};
    ds::ultimoAplicado = {}; ds::seleccion = {}; ds::poseido = false;
    ds::manualOriginal = false; ds::esperarAutomatico = false; ds::generacionPropia = 0;
    ds::enlaces = {s::manualNativo,
        s::escalarNativo<d::climaPrecipitacion>, s::escalarNativo<d::climaNubosidad>,
        s::escalarNativo<d::climaHumedad>, s::escalarNativo<d::climaViento>,
        s::escalarNativo<d::atmosferaNiebla>, s::escalarNativo<d::atmosferaNubes>,
        s::derivarNativo};
    poner(s::administrador, d::manual, uint8_t(manual));
    tiempoSimulado = 1000;
}
void observar() {
    ds::observarCliente(direccion(s::campo), s::baseFicticia + d::retornoCliente, tiempoSimulado);
}
void procesar(bool principal = true) {
    ds::procesarClima(direccion(s::administrador),
        s::baseFicticia + (principal ? d::retornoPrincipal : d::retornoSecundario),
        tiempoSimulado, principal);
}
ds::InstantaneaClima capturar() {
    ds::MarcoClima marco; ds::InstantaneaClima resultado; ds::AjustesClima parametros;
    assert(ds::abrirMarco(direccion(s::administrador), marco));
    assert(ds::leerInstantanea(marco, resultado, parametros));
    return resultado;
}
ds::AjustesClima objetivo() { return {.75f, .6f, .5f, 20.f, 3.f, false}; }
void activar() {
    observar(); assert(ds::climaDisponible());
    ds::fijarClima(objetivo()); ds::activarClima(true); assert(ds::climaActivo());
    procesar(); assert(ds::poseido); assert(ds::estado.load() == 3);
}

int main() {
    using namespace ds;
    assert(!iniciarClima()); assert(!climaDisponible()); assert(hooksCreados == 0);

    // Una preferencia cargada antes del primer campo espera sin perderse ni escribir.
    reiniciar(); fijarClima(objetivo()); activarClima(true);
    assert(climaActivo() && !climaDisponible());
    procesar(); assert(climaActivo() && !poseido && s::escrituras == 0);
    observar(); procesar(); assert(poseido && climaActivo());

    reiniciar();
    MarcoClima marco;
    assert(abrirMarco(direccion(s::administrador), marco));
    poner(s::constantes, d::ajusteInterfaz - d::hijoPadre, direccion(s::otroRecurso));
    assert(!abrirMarco(direccion(s::administrador), marco)); assert(s::escrituras == 0);
    poner(s::constantes, d::ajusteInterfaz - d::hijoPadre, direccion(s::recurso));
    poner(s::mundo, d::mundoAdministrador, uintptr_t(0));
    assert(!abrirMarco(direccion(s::administrador), marco));
    poner(s::mundo, d::mundoAdministrador, direccion(s::administrador));
    observarCliente(direccion(s::campo), 0, tiempoSimulado); assert(!climaDisponible());
    poner(s::buffers, d::buffersFase, uint8_t(7)); observar(); assert(!climaDisponible());
    poner(s::buffers, d::buffersFase, uint8_t(5));
    poner(s::campo, d::campoEtapa, d::etapaInvalida); observar(); assert(!climaDisponible());
    poner(s::campo, d::campoEtapa, uint16_t(42)); observar(); assert(climaDisponible());

    fijarClima(objetivo());
    auto invalido = objetivo(); invalido.viento = 61; fijarClima(invalido);
    assert(ajustesClima().viento == 20.f);
    invalido = objetivo(); invalido.precipitacion = std::numeric_limits<float>::quiet_NaN(); fijarClima(invalido);
    assert(ajustesClima().precipitacion == .75f);

    // Los huecos excluidos pueden contener cualquier byte y se conservan.
    poner(s::constantes, d::ajusteInterfaz + 0x15d, uint8_t(0xff));
    poner(s::constantes, d::ajusteInterfaz + 0x17d, uint8_t(0xfe));
    const auto antes = capturar();
    activarClima(true);
    procesarClima(direccion(s::administrador), 0, tiempoSimulado, true);
    assert(s::escrituras == 0 && !poseido);
    procesar();
    assert(poseido && climaActivo());
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaPrecipitacion) == .75f);
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaLluvia) == .75f);
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaNieve) == 0.f);
    assert(sacar<float>(s::atmosfera, d::ajusteInterfaz + d::atmosferaNiebla) == 3.f);
    poner(s::campo, d::campoMascara, uint32_t(1)); observar(); assert(estado.load() == 4);
    poner(s::constantes, d::ajusteInterfaz + d::climaFase, -1.f);
    procesar(false); assert(poseido && !fallo.load());
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaFase) == 1.f);
    // Otro hilo puede tener abierto el ajuste del menú. Aun así, el posthook
    // debe reponer las salidas que el productor nativo acaba de cambiar.
    poner(s::constantes, d::ajusteInterfaz + d::climaFase, -1.f);
    std::latch mutexTomado(1), liberarMutex(1);
    std::jthread editor([&] {
        std::lock_guard<std::mutex> cierre(mutexSolicitud);
        mutexTomado.count_down(); liberarMutex.wait();
    });
    mutexTomado.wait(); procesar(false);
    const bool faseRepuesta = sacar<float>(s::constantes, d::ajusteInterfaz + d::climaFase) == 1.f;
    liberarMutex.count_down(); editor.join();
    assert(faseRepuesta && poseido && !fallo.load());
    poner(s::constantes, d::ajusteInterfaz + 0x15d, uint8_t(0xab));
    activarClima(false); procesar();
    assert(!poseido && !climaActivo()); assert(capturar() == antes);
    assert(sacar<uint8_t>(s::administrador, d::manual) == 1);
    assert(sacar<uint8_t>(s::constantes, d::ajusteInterfaz + 0x15d) == 0xab);

    reiniciar(false); const auto naturalAntes = capturar(); activar();
    activarClima(false); procesar(false);
    assert(capturar() == naturalAntes); assert(sacar<uint8_t>(s::administrador, d::manual) == 0);
    assert(esperarAutomatico && estado.load() == 7);
    procesar(false); assert(esperarAutomatico);
    // El siguiente productor principal ya ejecutó su trabajo automático.
    poner(s::constantes, d::ajusteInterfaz + d::climaViento, 25.f);
    procesar(); assert(!esperarAutomatico && estado.load() == 2);
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaViento) == 25.f);

    reiniciar(false); activar(); activarClima(false); procesar(false);
    assert(esperarAutomatico);
    // La espera del productor anterior no pertenece al nuevo recurso, aunque
    // el manager nuevo ya venga en modo manual por otra política del juego.
    poner(s::bufferActivo, d::bufferActivo, uint8_t(0)); observar();
    poner(s::administrador, d::administradorResultado, direccion(s::otroRecurso) + d::ajusteInterfaz);
    poner(s::administrador, d::manual, uint8_t(1));
    poner(s::bufferActivo, d::bufferActivo, uint8_t(1)); observar(); procesar(false);
    assert(!esperarAutomatico && !poseido);
    activarClima(true); procesar(); assert(poseido && climaActivo());
    assert(sacar<float>(s::otrasConstantes, d::ajusteInterfaz + d::climaPrecipitacion) == .75f);

    reiniciar(false); activar(); activarClima(false); procesar(false);
    assert(esperarAutomatico);
    poner(s::campo, d::campoEtapa, uint16_t(43));
    poner(s::administrador, d::manual, uint8_t(1)); observar(); procesar(false);
    assert(!esperarAutomatico && !poseido);
    activarClima(true); procesar(); assert(poseido && climaActivo());

    reiniciar(); activar();
    auto nieve = objetivo(); nieve.nieve = true; fijarClima(nieve); procesar();
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaLluvia) == 0.f);
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaNieve) == .75f);
    poner(s::campo, d::campoMascara, uint32_t(2)); observar(); assert(estado.load() == 4);
    poner(s::constantes, d::ajusteInterfaz + d::climaPrecipitacion, .9f);
    const auto cuentaAjena = s::escrituras; procesar();
    assert(!poseido && !climaActivo() && estado.load() == 6);
    assert(s::escrituras == cuentaAjena);
    assert(sacar<float>(s::constantes, d::ajusteInterfaz + d::climaPrecipitacion) == .9f);

    reiniciar(); activar();
    const auto viejoAplicado = s::constantes;
    poner(s::administrador, d::administradorResultado, direccion(s::otroRecurso) + d::ajusteInterfaz);
    observar(); const auto cuentaCambio = s::escrituras; procesar();
    assert(!poseido && !climaActivo()); assert(s::escrituras == cuentaCambio);
    assert(s::constantes == viejoAplicado);
    assert(sacar<float>(s::otrasConstantes, d::ajusteInterfaz + d::climaPrecipitacion) == .2f);

    reiniciar(); const auto vencidoAntes = capturar(); activar();
    tiempoSimulado += d::vigenciaClienteMs + 1; actualizarClima();
    assert(!climaDisponible()); procesar();
    assert(!poseido && !climaActivo() && capturar() == vencidoAntes);

    reiniciar(); const auto antesBufferInactivo = capturar(); activar();
    poner(s::bufferActivo, d::bufferActivo, uint8_t(0)); observar();
    assert(!climaDisponible()); assert(ultimoCliente.load() == 0 && testigo.instante == 0);
    procesar();
    assert(!poseido && !climaActivo() && capturar() == antesBufferInactivo);

    reiniciar(); observar();
    poner(s::constantes, d::ajusteInterfaz + d::climaViento, std::numeric_limits<float>::infinity());
    activarClima(true); const auto cuentaInvalida = s::escrituras; procesar();
    assert(fallo.load() && !climaActivo() && !climaDisponible());
    assert(s::escrituras == cuentaInvalida); activarClima(true); assert(!climaActivo());

    reiniciar(); originalPrincipal.store(productorSimulado);
    hookPrincipal(s::administrador.data(), .125f);
    assert(llamadasProductor == 1 && deltaObservado == .125f);
    assert(!dentroClima);
    std::cout << "Pruebas simuladas de clima aprobadas: identidad, escalares, publicacion, restauracion y cambios ajenos\n";
}
