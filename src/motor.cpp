#include "motor.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include "funciones.hpp"
#include "inventario.hpp"
#include "delitos.hpp"
#include "regiones.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstdint>

namespace ds {
namespace {
using Movimiento = uint64_t(__fastcall*)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
Movimiento original{};
std::atomic_bool instalado{false}, bloqueado{false};
std::atomic_flag procesando = ATOMIC_FLAG_INIT;
uint64_t __fastcall movimiento(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g) {
    const auto resultado=original(a,b,c,d,e,f,g);
    // El prólogo y ABI provienen de teleport.cpp del fork fijado. No se escribe
    // al controlador recibido ni se presupone que sea exclusivamente del jugador.
    // El trabajo aplica sus propias identidades frescas y validación del realm.
    if(!listo.load(std::memory_order_acquire) || bloqueado.load() ||
       procesando.test_and_set(std::memory_order_acquire)) return resultado;
    struct Liberar { ~Liberar(){procesando.clear(std::memory_order_release);} } liberar;
    try {
        procesarInventario();
        actualizarDelitos();
        procesarRegiones();
    } catch(...) { bloqueado.store(true); }
    return resultado;
}
}
bool iniciarMotor() {
    if(instalado.load()) return true;
    const auto mh=MH_Initialize();
    if(mh!=MH_OK && mh!=MH_ERROR_ALREADY_INITIALIZED) return false;
    const auto destino=buscarUnico(datos::firmaTrabajoMotor);
    if(!destino || !ejecutable(destino)) return false;
    void* objetivo=reinterpret_cast<void*>(destino);
    if(MH_CreateHook(objetivo,reinterpret_cast<void*>(&movimiento),reinterpret_cast<void**>(&original))!=MH_OK) return false;
    if(MH_EnableHook(objetivo)!=MH_OK) { MH_RemoveHook(objetivo); original=nullptr; return false; }
    instalado.store(true); return true;
}
bool motorDisponible(){return instalado.load() && !bloqueado.load();}
const char* estadoMotor(){
    return bloqueado.load() ? "Trabajo del motor bloqueado por excepción; reinicia el juego." :
        instalado.load() ? "Cola instalada en el tick de movimiento publicado." :
        "no disponible: falta una coincidencia única del tick de movimiento; no se ejecutan ediciones.";
}
}
