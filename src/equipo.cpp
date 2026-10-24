#include "equipo.hpp"
#include "datos_juego.hpp"
#include "inventario_motor.hpp"
#include "inventario.hpp"
#include "memoria.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

// Adaptación propia de la investigación MIT de Trinity. Los layouts se
// centralizan en datos_juego.hpp; no se incorporan RVA de otro ejecutable.
namespace ds {
namespace {
namespace d = datos::equipo;
using Refrescar = void* (__fastcall*)(void*, int*);
Refrescar refrescar = nullptr;
std::atomic_bool instalado{false}, activo{false}, bloqueado{false};
std::atomic_flag procesando = ATOMIC_FLAG_INIT;
enum class Accion : uint8_t { Ninguna, Refinar };
std::atomic<Accion> pendiente{Accion::Ninguna};
std::mutex mutexEstado;
std::string estado = "Equipo experimental desactivado.";

void informar(const char* texto) {
    std::lock_guard<std::mutex> lk(mutexEstado); estado = texto;
}
bool sumar(uintptr_t base, size_t offset, uintptr_t& resultado) {
    if (base < 0x10000000 || offset > std::numeric_limits<uintptr_t>::max() - base) return false;
    resultado = base + offset; return true;
}
template<class T> bool campo(uintptr_t base, size_t offset, T& valor) {
    uintptr_t direccion = 0;
    return sumar(base, offset, direccion) && leer(direccion, valor);
}
bool identidad(const ParEquipo& par) {
    uint64_t a = 0, b = 0; uint16_t ta = d::tipoVacio, tb = d::tipoVacio;
    return par.cliente != par.autoridad && par.instancia && par.instancia != UINT64_MAX &&
        par.tipo != d::tipoVacio && campo(par.cliente, d::instancia, a) &&
        campo(par.autoridad, d::instancia, b) && a == par.instancia && b == a &&
        campo(par.cliente, d::tipo, ta) && campo(par.autoridad, d::tipo, tb) &&
        ta == par.tipo && tb == ta;
}
bool modificable(uintptr_t direccion, size_t n) {
    if (!n || direccion > UINTPTR_MAX - n) return false;
    const auto fin = direccion + n;
    while (direccion < fin) {
        MEMORY_BASIC_INFORMATION memoria{};
        if (!VirtualQuery(reinterpret_cast<void*>(direccion), &memoria, sizeof(memoria)) ||
            memoria.State != MEM_COMMIT || (memoria.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const DWORD permiso = memoria.Protect & 0xFF;
        if (permiso != PAGE_READWRITE && permiso != PAGE_WRITECOPY) return false;
        const auto siguiente = reinterpret_cast<uintptr_t>(memoria.BaseAddress) + memoria.RegionSize;
        if (siguiente <= direccion) return false;
        direccion = siguiente;
    }
    return true;
}
uintptr_t componentePara(uintptr_t actor) {
    uintptr_t sub = 0, componente = 0, propietario = 0;
    if (!campo(actor, d::subActor, sub) || !campo(sub, d::componente, componente) ||
        !campo(componente, d::propietario, propietario) || propietario != actor) return 0;
    return componente;
}
bool controladorClienteValido(uintptr_t componente, uintptr_t actorEsperado) {
    uintptr_t actor = 0, controlador = 0, pawn = 0;
    return campo(componente, d::propietario, actor) && actor == actorEsperado &&
        campo(actor, trinity::game::kOff_Owner_Possessor, controlador) &&
        campo(controlador, trinity::game::kOff_Possessor_Pawn, pawn) && pawn >= 0x10000000;
}
bool tablaEquipados(uintptr_t componente, uintptr_t& datos, uint32_t& cantidad) {
    uintptr_t tabla = 0;
    return campo(componente, d::tabla, tabla) && campo(tabla, d::datosTabla, datos) &&
        datos >= 0x10000000 && campo(tabla, d::cantidadTabla, cantidad) &&
        cantidad <= d::maximoEquipados;
}
struct Equipado { uintptr_t valor = 0; uint64_t id = 0; uint16_t tipo = 0, etiqueta = 0; };
bool leerVestidos(uintptr_t componente, std::vector<Equipado>& salida) {
    uintptr_t datos = 0; uint32_t cantidad = 0;
    if (!tablaEquipados(componente, datos, cantidad)) return false;
    std::unordered_set<uint16_t> etiquetas;
    for (uint32_t i = 0; i < cantidad; ++i) {
        uintptr_t valor = 0; Equipado item{};
        if (!sumar(datos, static_cast<size_t>(i) * d::pasoEquipado, valor) ||
            !campo(valor, d::tipo, item.tipo)) return false;
        if (item.tipo == d::tipoVacio) continue;
        if (!campo(valor, d::instancia, item.id) || !campo(valor, d::etiquetaEquipado, item.etiqueta) ||
            !item.id || item.id == UINT64_MAX || !etiquetas.insert(item.etiqueta).second) return false;
        item.valor = valor; salida.push_back(item);
    }
    return true;
}
bool agregarVestidos(std::vector<ParEquipo>& pares, uintptr_t cliente, uintptr_t autoridad,
                    ClasificarEquipo clasificar, size_t& omitidos) {
    if (!clasificar) return false;
    std::vector<Equipado> a, b;
    if (!leerVestidos(cliente, a) || !leerVestidos(autoridad, b)) return false;
    for (const auto& item : a) {
        auto otro = std::find_if(b.begin(), b.end(), [&](const Equipado& candidato) {
            return candidato.etiqueta == item.etiqueta && candidato.id == item.id && candidato.tipo == item.tipo;
        });
        bool refina = false, ranuras = false; uint16_t maximo = 0;
        if (otro == b.end() || !clasificar(item.tipo, &refina, &ranuras, &maximo)) { ++omitidos; continue; }
        // La fuente más reciente excluye linterna, máscara, mochila, brazalete
        // Axiom y equipo de montura: no tienen una curva de refinamiento.
        const bool etiquetaRefinable = item.etiqueta <= 13 || item.etiqueta == 16;
        pares.push_back({item.valor, otro->valor, item.id, item.tipo, refina && etiquetaRefinable, ranuras, maximo});
    }
    return true;
}

struct Cambio {
    uintptr_t direccion = 0;
    ParEquipo propietario{};
    size_t longitud = 0;
    std::array<uint8_t, 64> antes{}, despues{};
};
bool planificar(std::vector<Cambio>& cambios, const ParEquipo& propietario,
                 uintptr_t base, size_t offset, const void* nuevo, size_t n) {
    Cambio cambio{};
    if (!nuevo || !n || n > cambio.antes.size() || !sumar(base, offset, cambio.direccion) ||
        !modificable(cambio.direccion, n) || !leer(cambio.direccion, cambio.antes.data(), n)) return false;
    std::memcpy(cambio.despues.data(), nuevo, n); cambio.longitud = n; cambio.propietario = propietario;
    if (std::memcmp(cambio.antes.data(), nuevo, n) == 0) return true;
    for (const auto& previo : cambios) {
        if (cambio.direccion < previo.direccion + previo.longitud &&
            previo.direccion < cambio.direccion + n) return false;
    }
    cambios.push_back(cambio); return true;
}
template<class T> bool planificarCampo(std::vector<Cambio>& cambios, const ParEquipo& propietario,
                                      uintptr_t base, size_t offset, const T& nuevo) {
    return planificar(cambios, propietario, base, offset, &nuevo, sizeof(nuevo));
}
bool planificarRefinar(std::vector<Cambio>& cambios, const ParEquipo& par) {
    uint16_t a = 0, b = 0;
    const uint16_t maximo = par.refinamientoMaximo;
    if (!maximo || maximo > d::refinamientoMaximo) return false;
    if (!campo(par.cliente, d::refinamiento, a) || !campo(par.autoridad, d::refinamiento, b) ||
        a != b || a > maximo) return false;
    return planificarCampo(cambios, par, par.autoridad, d::refinamiento, maximo) &&
        planificarCampo(cambios, par, par.cliente, d::refinamiento, maximo);
}
bool coincide(const Cambio& cambio, bool nuevo) {
    std::array<uint8_t, 64> leido{};
    const auto& esperado = nuevo ? cambio.despues : cambio.antes;
    return leer(cambio.direccion, leido.data(), cambio.longitud) &&
        std::memcmp(leido.data(), esperado.data(), cambio.longitud) == 0;
}
bool vigente(const Cambio& cambio) { return identidad(cambio.propietario); }
bool escribirCambio(const Cambio& cambio, bool nuevo) {
    const auto& valor = nuevo ? cambio.despues : cambio.antes;
    return vigente(cambio) &&
        escribirDatos(cambio.direccion, valor.data(), cambio.longitud) && coincide(cambio, nuevo);
}
// ABI publicado por Trinity. Esta envoltura sólo contiene locales POD.
bool refrescarProtegido(uintptr_t componente) {
    if (!refrescar || !componente) return false;
    __try { int error = 0; refrescar(reinterpret_cast<void*>(componente), &error); return error == 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool restaurar(const std::vector<Cambio>& cambios, size_t hechos) {
    bool ok = true;
    while (hechos) { --hechos; ok = escribirCambio(cambios[hechos], false) && ok; }
    return ok;
}
void bloquear(bool restaurado) {
    bloqueado.store(true); activo.store(false); pendiente.store(Accion::Ninguna);
    informar(restaurado ? "Equipo bloqueado: falló la operación; se restauraron los bytes originales. Reinicia antes de continuar." :
        "Equipo bloqueado: falló la operación y no se pudo verificar toda la restauración. Recarga el respaldo sin guardar.");
}
bool encolar(Accion accion) {
    if (!equipoActivo()) return false;
    auto esperado = Accion::Ninguna;
    if (!pendiente.compare_exchange_strong(esperado, accion)) return false;
    informar("Operación experimental pendiente del tick seguro del motor."); return true;
}
}
bool iniciarEquipo() {
    if (instalado.load()) return true;
    if (!disenoInventarioLegadoVerificado()) {
        informar("no disponible: faltan los dos testigos de layout C0 verificados; no se instalaron escrituras de refinamiento"); return false;
    }
    const auto funcion = buscarUnico(d::firmaRefrescar);
    if (!funcion || !ejecutable(funcion)) {
        informar("Equipo no disponible: firma única de actualización no resuelta para el perfil Trinity."); return false;
    }
    refrescar = reinterpret_cast<Refrescar>(funcion); instalado.store(true);
    informar("Refinamiento experimental listo y apagado para el layout C0 verificado. Respeta el límite por tipo; estadísticas y guardado requieren validación en el juego.");
    return true;
}
bool refinarDisponible() { return instalado.load() && !bloqueado.load() && disenoInventarioLegadoVerificado(); }
bool equipoActivo() { return activo.load() && refinarDisponible(); }
bool activarEquipo(bool valor) {
    if (valor && !refinarDisponible()) return false;
    activo.store(valor);
    if (!valor) { pendiente.store(Accion::Ninguna); if (!bloqueado.load()) informar("Editor de equipo desactivado."); }
    return true;
}
const char* estadoEquipo() {
    thread_local std::string copia;
    std::lock_guard<std::mutex> lk(mutexEstado); copia = estado; return copia.c_str();
}
bool solicitarRefinarTodo() { return encolar(Accion::Refinar); }
void cancelarSolicitudesEquipo() { pendiente.store(Accion::Ninguna); }
void procesarEquipo(const ParEquipo* mochila, size_t cantidad, ClasificarEquipo clasificar) {
    if (!equipoActivo() || procesando.test_and_set(std::memory_order_acquire)) return;
    struct Liberar { ~Liberar() { procesando.clear(std::memory_order_release); } } liberar;
    const auto accion = pendiente.exchange(Accion::Ninguna);
    if (accion == Accion::Ninguna) return;
    try {
        if (cantidad > 8192 || (cantidad && !mochila) || !clasificar) {
            informar("Operación rechazada: faltan pares de objetos o el clasificador del tipo."); return;
        }
        const auto clienteActor = trinity::game::Inventory::ClientCharacterAddr();
        const auto autoridadActor = trinity::game::Inventory::ServerCharacterAddr();
        const auto cliente = componentePara(clienteActor), autoridad = componentePara(autoridadActor);
        if (!cliente || !autoridad || clienteActor == autoridadActor || cliente == autoridad ||
            !controladorClienteValido(cliente, clienteActor)) {
            informar("Esperando los componentes de equipo de ambos personajes; no se modificó ningún objeto."); return;
        }
        std::vector<ParEquipo> pares;
        if (cantidad) pares.assign(mochila, mochila + cantidad);
        size_t omitidos = 0;
        if (!agregarVestidos(pares, cliente, autoridad, clasificar, omitidos)) {
            informar("Operación rechazada: no coincide la tabla de equipo vestido con el perfil publicado."); return;
        }
        std::unordered_set<uint64_t> ids;
        std::unordered_set<uintptr_t> direcciones;
        std::vector<Cambio> cambios;
        size_t modificados = 0, sinCambios = 0;
        for (const auto& par : pares) {
            if (!identidad(par) || !ids.insert(par.instancia).second ||
                !direcciones.insert(par.cliente).second || !direcciones.insert(par.autoridad).second) {
                informar("Operación rechazada: identidad ambigua o un objeto cambió entre los dos mirrors."); return;
            }
            bool refina = false, ranuras = false; uint16_t maximo = 0;
            if (!clasificar(par.tipo, &refina, &ranuras, &maximo) ||
                (!refina || maximo != par.refinamientoMaximo)) {
                ++omitidos; continue;
            }
            if (!par.refinable || !maximo || maximo > d::refinamientoMaximo) { ++omitidos; continue; }
            const auto inicio = cambios.size();
            const bool ok = planificarRefinar(cambios, par);
            if (!ok) { cambios.resize(inicio); ++omitidos; continue; }
            if (cambios.size() == inicio) ++sinCambios; else ++modificados;
        }
        if (cambios.empty()) {
            char texto[256]{};
            std::snprintf(texto, sizeof(texto), "No se escribieron datos: %zu objetos ya al máximo; %zu excluidos por tipo o layout/capacidad no compatibles.", sinCambios, omitidos);
            informar(texto); return;
        }
        // Se valida el lote completo antes de la primera escritura; ningún
        // cambio usa un offset inferido a partir de otra versión del juego.
        for (const auto& cambio : cambios) if (!vigente(cambio) || !coincide(cambio, false)) {
            informar("Operación cancelada: el equipo cambió durante la validación. No se escribieron datos."); return;
        }
        size_t hechos = 0;
        for (const auto& cambio : cambios) {
            ++hechos; // también restaurar una escritura que pudiera fallar a mitad.
            if (!equipoActivo() || !escribirCambio(cambio, true)) { bloquear(restaurar(cambios, hechos)); return; }
        }
        // La fuente MIT más reciente (fork 96e72fe, equipment.cpp:2788-2824)
        // advierte de acceso a estructuras de render en esta función: sólo se
        // llama para el componente visual con un controlador local válido.
        // La autoridad recibe datos y readback, no una llamada de render.
        const bool clienteActualizado = controladorClienteValido(cliente, clienteActor) && refrescarProtegido(cliente);
        if (!clienteActualizado || componentePara(clienteActor) != cliente ||
            componentePara(autoridadActor) != autoridad) {
            bloquear(restaurar(cambios, hechos)); return;
        }
        for (const auto& cambio : cambios) if (!vigente(cambio) || !coincide(cambio, true)) {
            bloquear(restaurar(cambios, hechos)); return;
        }
        char texto[384]{};
        std::snprintf(texto, sizeof(texto),
            "Experimental: máximo por tipo (hasta 10) escrito y leído en ambos mirrors de %zu objetos; %zu ya al máximo; %zu excluidos. Actualización visual solicitada; estadísticas y guardado pendientes de comprobar.",
            modificados, sinCambios, omitidos);
        informar(texto);
    } catch (...) {
        // Una excepción C++ en planificación no tiene escrituras; durante el
        // commit las lecturas/escrituras y llamadas nativas ya están protegidas.
        bloquear(false);
    }
}
}
