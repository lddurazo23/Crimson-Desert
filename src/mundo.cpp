#include "mundo.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include "inventario.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace ds {
namespace {
using TickReloj = void(__fastcall*)(void*, float, float);
TickReloj originalReloj = nullptr;
uintptr_t relojCliente = 0, relojServidor = 0, globalMotorSol = 0;
std::atomic_bool relojListo{false}, relojActivado{false}, relojCongelado{true};
std::atomic_int solicitudHora{-1};
std::atomic_int estadoReloj{0};
std::atomic_flag procesandoTiempo = ATOMIC_FLAG_INIT;
// Los datos siguientes pertenecen únicamente al hilo que ejecuta TickReloj.
uintptr_t administradorSostenido = 0;
float limiteInferiorAnterior = 0, limiteSuperiorAnterior = 24, horaSostenida = 12;
float ultimaHoraAplicada = -1;
bool limitesAplicados = false;
thread_local bool dentroDelReloj = false;

uintptr_t administradorSol() {
    uintptr_t motor = 0, administrador = 0;
    if (!leer(globalMotorSol, motor) || !motor ||
        !leer(motor + datos::offsetAdministradorSol, administrador)) return 0;
    return administrador;
}
bool horaValida(float valor) {
    return std::isfinite(valor) && valor >= 0.0f && valor <= 24.0f;
}
bool relojValido(uintptr_t direccion) {
    int dia = -1, hora = -1, minuto = -1, segundo = -1;
    return direccion && leer(direccion + datos::offsetDia, dia) && dia >= 0 && dia < 10000000 &&
        leer(direccion + datos::offsetHora, hora) && hora >= 0 && hora < 24 &&
        leer(direccion + datos::offsetMinuto, minuto) && minuto >= 0 && minuto < 60 &&
        leer(direccion + datos::offsetSegundo, segundo) && segundo >= 0 && segundo < 60;
}
void restaurarLimites(uintptr_t administradorActual) {
    if (!limitesAplicados) return;
    // Al cambiar de administrador no se escribe al objeto anterior: podría haberse liberado.
    if (administradorActual == administradorSostenido) {
        float inferior = -1, superior = -1;
        if (leer(administradorActual + datos::offsetLimiteSolInferior, inferior) &&
            leer(administradorActual + datos::offsetLimiteSolSuperior, superior) &&
            inferior == ultimaHoraAplicada && superior == ultimaHoraAplicada) {
            escribir(administradorActual + datos::offsetLimiteSolInferior, limiteInferiorAnterior);
            escribir(administradorActual + datos::offsetLimiteSolSuperior, limiteSuperiorAnterior);
        }
    }
    administradorSostenido = 0;
    limitesAplicados = false;
    ultimaHoraAplicada = -1;
}
bool sostenerSol(uintptr_t administrador) {
    if (!administrador) return false;
    if (limitesAplicados && administradorSostenido != administrador) {
        limitesAplicados = false;
        administradorSostenido = 0;
        ultimaHoraAplicada = -1;
    }
    if (!limitesAplicados) {
        float actual = -1, inferior = -1, superior = -1;
        if (!leer(administrador + datos::offsetHoraSol, actual) || !horaValida(actual) ||
            !leer(administrador + datos::offsetLimiteSolInferior, inferior) || !horaValida(inferior) ||
            !leer(administrador + datos::offsetLimiteSolSuperior, superior) || !horaValida(superior) ||
            inferior > superior) return false;
        limiteInferiorAnterior = inferior;
        limiteSuperiorAnterior = superior;
        horaSostenida = actual;
        administradorSostenido = administrador;
        limitesAplicados = true;
    }
    float inferiorAntes = -1, superiorAntes = -1;
    if (!leer(administrador + datos::offsetLimiteSolInferior, inferiorAntes) ||
        !leer(administrador + datos::offsetLimiteSolSuperior, superiorAntes) ||
        !horaValida(inferiorAntes) || !horaValida(superiorAntes)) return false;
    if (!escribir(administrador + datos::offsetLimiteSolInferior, horaSostenida)) {
        restaurarLimites(administrador);
        return false;
    }
    if (!escribir(administrador + datos::offsetLimiteSolSuperior, horaSostenida)) {
        escribir(administrador + datos::offsetLimiteSolInferior, inferiorAntes);
        return false;
    }
    ultimaHoraAplicada = horaSostenida;
    return true;
}
bool aplicarHora(int totalMinutos) {
    // Se conserva el día de cada dominio. No se manipulan días ni misiones.
    const int hora = totalMinutos / 60, minuto = totalMinutos % 60;
    if (!relojValido(relojCliente) || !relojValido(relojServidor)) return false;
    int anteriores[4]{};
    if (!leer(relojCliente + datos::offsetHora, anteriores[0]) ||
        !leer(relojCliente + datos::offsetMinuto, anteriores[1]) ||
        !leer(relojServidor + datos::offsetHora, anteriores[2]) ||
        !leer(relojServidor + datos::offsetMinuto, anteriores[3])) return false;
    const uintptr_t destinos[4]{relojCliente + datos::offsetHora, relojCliente + datos::offsetMinuto,
        relojServidor + datos::offsetHora, relojServidor + datos::offsetMinuto};
    const int valores[4]{hora, minuto, hora, minuto};
    unsigned escritos = 0;
    for (; escritos < 4; ++escritos) if (!escribir(destinos[escritos], valores[escritos])) break;
    if (escritos != 4) {
        while (escritos) { --escritos; escribir(destinos[escritos], anteriores[escritos]); }
        return false;
    }
    if (limitesAplicados) horaSostenida = static_cast<float>(hora) + static_cast<float>(minuto) / 60.0f;
    return true;
}
void actualizarTiempoEnJuego() {
    const uintptr_t administrador = administradorSol();
    if (!relojActivado.load(std::memory_order_relaxed)) {
        restaurarLimites(administrador);
        solicitudHora.store(-1);
        return;
    }
    if (!relojValido(relojCliente) || !relojValido(relojServidor) || !administrador) {
        estadoReloj.store(2);
        return;
    }
    const bool congelado = relojCongelado.load(std::memory_order_relaxed);
    if (congelado) {
        if (!sostenerSol(administrador)) { estadoReloj.store(3); relojActivado.store(false); return; }
    } else restaurarLimites(administrador);
    const int pendiente = solicitudHora.exchange(-1);
    if (pendiente >= 0 && !aplicarHora(pendiente)) {
        estadoReloj.store(3); relojActivado.store(false); restaurarLimites(administrador); return;
    }
    if (congelado && !sostenerSol(administrador)) {
        estadoReloj.store(3); relojActivado.store(false); return;
    }
    estadoReloj.store(1);
}
void __fastcall hookReloj(void* administrador, float delta, float otroDelta) {
    if (dentroDelReloj) { originalReloj(administrador, delta, otroDelta); return; }
    dentroDelReloj = true;
    const bool congelar = relojActivado.load(std::memory_order_relaxed) &&
        relojCongelado.load(std::memory_order_relaxed) && relojValido(relojCliente) &&
        relojValido(relojServidor) && administradorSol();
    originalReloj(administrador, congelar ? 0.0f : delta, otroDelta);
    // Dos dominios pueden llamar el mismo tick. Nunca se bloquea un hilo del juego.
    if (!procesandoTiempo.test_and_set(std::memory_order_acquire)) {
        actualizarTiempoEnJuego();
        procesandoTiempo.clear(std::memory_order_release);
    }
    dentroDelReloj = false;
}
}

bool iniciarMundo() {
    if (relojListo.load()) return true;
    const auto inicializado = MH_Initialize();
    if (inicializado != MH_OK && inicializado != MH_ERROR_ALREADY_INITIALIZED) return false;
    const uintptr_t firmaRelojes = buscarUnico(datos::firmaRelojes);
    const uintptr_t firmaMotor = buscarUnico(datos::firmaMotorSol);
    const uintptr_t tick = buscarUnico(datos::firmaTickReloj);
    if (!firmaRelojes || !firmaMotor || !tick) return false;
    relojServidor = resolverRip(firmaRelojes + datos::instruccionRelojServidor,
        datos::desplazamientoReloj, datos::longitudInstruccionReloj);
    relojCliente = resolverRip(firmaRelojes + datos::instruccionRelojCliente,
        datos::desplazamientoReloj, datos::longitudInstruccionReloj);
    globalMotorSol = resolverRip(firmaMotor + datos::instruccionMotorSol,
        datos::desplazamientoMotorSol, datos::longitudInstruccionMotorSol);
    if (!relojServidor || !relojCliente || relojServidor == relojCliente || !globalMotorSol || !ejecutable(tick)) return false;
    if (MH_CreateHook(reinterpret_cast<void*>(tick), reinterpret_cast<void*>(&hookReloj),
        reinterpret_cast<void**>(&originalReloj)) != MH_OK) return false;
    if (MH_EnableHook(reinterpret_cast<void*>(tick)) != MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(tick)); originalReloj = nullptr; return false;
    }
    relojListo.store(true);
    return true;
}
bool tiempoDisponible() { return relojListo.load() && estadoReloj.load()!=3; }
const char* estadoTiempo() {
    if (!relojListo.load()) return "No disponible: faltan datos o los patrones no son únicos";
    switch (estadoReloj.load()) {
    case 1: return "Datos públicos; reloj y sol conectados (sin prueba en esta versión)";
    case 2: return "Esperando que se cargue el mundo";
    case 3: return "Desactivado: la estructura del reloj cambió o rechazó la escritura";
    default: return "Datos públicos; listo para activar después de cargar una partida";
    }
}
void activarTiempo(bool activar) { relojActivado.store(activar && tiempoDisponible()); }
bool tiempoActivo() { return relojActivado.load(); }
void fijarHora(int hora, int minuto) {
    if (relojListo.load() && hora >= 0 && hora < 24 && minuto >= 0 && minuto < 60)
        solicitudHora.store(hora * 60 + minuto);
}
void congelarTiempo(bool congelar) { relojCongelado.store(congelar); }
// No se escribe memoria desde el renderizador; el hook drena las solicitudes.
}
