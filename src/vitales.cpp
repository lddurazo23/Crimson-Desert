#include "vitales.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>

namespace ds {
namespace {
// Adaptación MIT de la ruta vital publicada por XeTrinityz/Trinity.
// Solo el personaje actualmente controlado se modifica; los NPC no se incluyen.
using ConfirmarVital = int64_t(__fastcall*)(void*, int64_t, int64_t, uint16_t);
ConfirmarVital original = nullptr;
std::atomic_bool instalado = false;
std::atomic_bool opciones[3]{};
std::atomic<ULONGLONG> ultimoPersonaje = 0;
std::atomic_bool falloEscritura = false;
std::atomic_uintptr_t globalPersonajes = 0;
const char* errorInstalacion = "no disponible: las firmas vitales todavía no se han resuelto";

bool puntero(uintptr_t direccion, uintptr_t& destino) {
    return leer(direccion, destino) && destino >= 0x10000;
}
bool etiquetaProtagonista(uintptr_t propietario) {
    uintptr_t descriptor = 0;
    uint8_t etiqueta = 0;
    return puntero(propietario + datos::descriptorPersonaje, descriptor) &&
        leer(descriptor + datos::etiquetaDescriptor, etiqueta) &&
        (((static_cast<unsigned>(etiqueta) - 1u) & 0xF7u) == 0);
}
uintptr_t resolverEstadisticas(uintptr_t* salidaPropietario = nullptr, uintptr_t* salidaActor = nullptr) {
    if (salidaPropietario) *salidaPropietario = 0;
    if (salidaActor) *salidaActor = 0;
    uintptr_t indirecto = 0, administrador = 0, lista = 0;
    uint32_t cantidad = 0;
    if (!puntero(globalPersonajes.load(std::memory_order_acquire), indirecto) || !puntero(indirecto, administrador) ||
        !puntero(administrador + datos::listaPersonajes, lista) ||
        !leer(administrador + datos::cantidadPersonajes, cantidad) ||
        cantidad == 0 || cantidad > datos::maximoPersonajes) return 0;

    // La clase se reconoce por el descriptor publicado, sin fijar direcciones de vtables.
    uintptr_t clase = 0;
    for (uint32_t i = 0; i < cantidad; ++i) {
        uintptr_t propietario = 0;
        if (puntero(lista + sizeof(uintptr_t) * i, propietario) && etiquetaProtagonista(propietario) &&
            puntero(propietario, clase)) break;
    }
    // La vtable está en datos de solo lectura; se valida su primera función.
    uintptr_t primerMetodo = 0;
    if (!clase || !puntero(clase, primerMetodo) || !ejecutable(primerMetodo)) return 0;
    uintptr_t elegido = 0, actorElegido = 0, estadisticas = 0;
    for (uint32_t i = 0; i < cantidad; ++i) {
        uintptr_t propietario = 0, vtable = 0, controlador = 0, retorno = 0;
        if (!puntero(lista + sizeof(uintptr_t) * i, propietario) ||
            !puntero(propietario, vtable) || vtable != clase ||
            !puntero(propietario + datos::controladorPersonaje, controlador) ||
            !puntero(controlador + datos::personajeControlado, retorno) || retorno != propietario) continue;
        // Dos resultados hacen ambigua la identidad: no se escribe en ninguno.
        if (elegido) return 0;
        uintptr_t actor = 0, marcador = 0, vitales = 0, arreglo = 0;
        int32_t tipo = -1;
        if (!puntero(propietario + datos::actorPersonaje, actor) ||
            !puntero(actor + datos::marcadorActor, marcador) ||
            !puntero(marcador + datos::vitalesMarcador, vitales) ||
            !puntero(vitales + datos::arregloVitales, arreglo) ||
            !leer(arreglo + datos::tipoVital, tipo) || tipo != datos::tipoSalud) continue;
        elegido = propietario;
        actorElegido = actor;
        estadisticas = arreglo;
    }
    if (estadisticas) {
        if (salidaPropietario) *salidaPropietario = elegido;
        if (salidaActor) *salidaActor = actorElegido;
    }
    return estadisticas;
}
int categoria(int32_t tipo) {
    if (tipo == datos::tipoSalud) return 0;
    for (int32_t t : datos::tiposStamina) if (tipo == t) return 1;
    for (int32_t t : datos::tiposEspiritu) if (tipo == t) return 2;
    return -1;
}
bool fijarVital(uintptr_t entrada, int esperado) {
    int32_t tipo = -1;
    int64_t base = 0, maximo = 0, anterior = 0, normalizado = 0;
    if (!leer(entrada + datos::tipoVital, tipo) || categoria(tipo) != esperado ||
        !leer(entrada + datos::baseVital, base) || !leer(entrada + datos::maximoVital, maximo) ||
        !leer(entrada + datos::actualVital, anterior) ||
        !leer(entrada + datos::normalizadoVital, normalizado) || base < 0 || maximo < 0) return false;
    const int64_t lleno = std::max(base, maximo);
    // Un medidor sin capacidad todavía no está listo; no se inventa una capacidad.
    if (lleno == 0) return true;
    // Ambos operandos son int64 no negativos: lleno <= INT64_MAX y lleno >= base.
    // Por lo tanto, lleno - base tampoco puede desbordarse ni ser negativo.
    const int64_t diferencia = lleno - base;
    if (!escribir(entrada + datos::actualVital, lleno)) return false;
    if (escribir(entrada + datos::normalizadoVital, diferencia)) return true;
    // Si una página desaparece, se intenta revertir el primer campo y se desactiva todo.
    escribir(entrada + datos::actualVital, anterior);
    return false;
}
int64_t __fastcall confirmarHook(void* entrada, int64_t tiempo, int64_t objetivo, uint16_t bandera) {
    thread_local bool dentro = false;
    if (dentro) return original(entrada, tiempo, objetivo, bandera);
    struct Salir { bool& marca; ~Salir() { marca = false; } } salir{dentro};
    dentro = true;
    const int64_t resultado = original(entrada, tiempo, objetivo, bandera);
    if (!instalado.load(std::memory_order_acquire) || falloEscritura.load()) return resultado;
    const uintptr_t direccion = reinterpret_cast<uintptr_t>(entrada);
    int32_t tipo = -1;
    if (!leer(direccion + datos::tipoVital, tipo)) return resultado;
    const int clase = categoria(tipo);
    if (clase < 0 || !opciones[clase].load(std::memory_order_relaxed)) return resultado;
    const uintptr_t arreglo = resolverEstadisticas();
    if (arreglo) {
        ultimoPersonaje.store(GetTickCount64(), std::memory_order_relaxed);
        for (size_t i = 0; i < datos::cantidadEntradasVitales; ++i) {
            if (direccion != arreglo + datos::pasoVital * i) continue;
            if (!fijarVital(direccion, clase)) {
                falloEscritura.store(true);
                for (auto& opcion : opciones) opcion.store(false);
                OutputDebugStringW(L"DesiertoSeguro: escritura vital rechazada; opciones desactivadas.\n");
            }
            break;
        }
    }
    return resultado;
}
}
bool iniciarResolucionPersonaje() {
    if (globalPersonajes.load(std::memory_order_acquire)) return true;
    uintptr_t acuerdo = 0;
    size_t votos = 0;
    for (const char* ancla : datos::anclasPersonajes) {
        const uintptr_t sitio = buscarUnico(ancla);
        if (!sitio) continue;
        const uintptr_t resuelto = resolverRip(sitio, 3, 7);
        if (!resuelto) continue;
        if (acuerdo && resuelto != acuerdo) {
            errorInstalacion = "no disponible: las firmas del personaje no coinciden";
            return false;
        }
        acuerdo = resuelto;
        ++votos;
    }
    if (votos < 2) {
        errorInstalacion = "no disponible: faltan al menos dos firmas únicas del personaje";
        return false;
    }
    globalPersonajes.store(acuerdo, std::memory_order_release);
    return true;
}
bool resolverPersonajeControlado(uintptr_t& propietario, uintptr_t& actor) {
    return resolverEstadisticas(&propietario, &actor) != 0;
}
uintptr_t propietarioControlado() {
    uintptr_t propietario = 0;
    return resolverEstadisticas(&propietario, nullptr) ? propietario : 0;
}
uintptr_t actorControlado() {
    uintptr_t actor = 0;
    return resolverEstadisticas(nullptr, &actor) ? actor : 0;
}
bool iniciarVitales() {
    if (instalado.load()) return true;
    if (!iniciarResolucionPersonaje()) return false;
    const uintptr_t confirmar = buscarUnico(datos::firmaVital);
    if (!confirmar || !ejecutable(confirmar)) {
        errorInstalacion = "no disponible: falta la firma única de salud, stamina y espíritu";
        return false;
    }
    const MH_STATUS inicio = MH_Initialize();
    if (inicio != MH_OK && inicio != MH_ERROR_ALREADY_INITIALIZED) {
        errorInstalacion = "no disponible: MinHook no pudo iniciar"; return false;
    }
    void* destino = reinterpret_cast<void*>(confirmar);
    if (MH_CreateHook(destino, reinterpret_cast<void*>(&confirmarHook), reinterpret_cast<void**>(&original)) != MH_OK) {
        errorInstalacion = "no disponible: no se pudo instalar el hook de vitales"; return false;
    }
    instalado.store(true, std::memory_order_release);
    if (MH_EnableHook(destino) != MH_OK) {
        instalado.store(false);
        MH_RemoveHook(destino);
        errorInstalacion = "no disponible: no se pudo activar el hook de vitales"; return false;
    }
    return true;
}
void activarVital(size_t i, bool activo) {
    if (i < 3) opciones[i].store(activo && instalado.load() && !falloEscritura.load());
}
bool vitalDisponible(size_t i) { return i < 3 && instalado.load() && !falloEscritura.load(); }
bool vitalActivo(size_t i) { return i < 3 && opciones[i].load(); }
const char* estadoVital(size_t i) {
    if (i >= 3) return "opción inexistente";
    if (falloEscritura.load()) return "no disponible: escritura rechazada; reinicia y revisa los datos";
    if (!instalado.load()) return errorInstalacion;
    if (!opciones[i].load()) return "disponible; desactivado";
    if (GetTickCount64() - ultimoPersonaje.load() > 5000) return "activado; esperando personaje y cambios de estadística";
    return "activado; personaje detectado";
}
}
