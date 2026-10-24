#include "delitos.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include "vitales.hpp"
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace ds {
// Adaptador al resolvedor por nombre publicado de Trinity; no instala hooks de IA.
uintptr_t resolverTablaPublica(const char* nombre, bool indirecta);
namespace {
namespace d = datos::delitos;
enum class Estado { SinDatos, SinTablas, Listo, Esperando, Activo, Restaurando,
                    Rechazado, RestauracionPendiente, Conflicto };
enum class Vigencia { Si, Cambio, Ilegible };
std::atomic<Estado> estado{Estado::SinDatos};
std::atomic_bool listo{false}, solicitado{false}, aplicado{false}, rechazado{false};
std::atomic_flag procesando = ATOMIC_FLAG_INIT;
std::array<uintptr_t, std::size(d::tablas)> globales{};
struct Vista {
    size_t indice{};
    uintptr_t tabla{}, filas{};
    uint32_t cantidad{};
};
struct CampoGuardado {
    CampoDelito dato{};
    std::array<uint8_t, 8> anterior{}, propio{};
    bool escrito{};
};
struct FilaGuardada {
    Vista vista{};
    uint32_t indice{};
    uint64_t identidad{};
    uintptr_t fila{};
    std::vector<CampoGuardado> campos;
};
// Solo actualizarDelitos() accede a las copias; el menú usa átomos.
std::vector<FilaGuardada> copias;

size_t ancho(TipoDatoDelito tipo) {
    switch (tipo) {
    case TipoDatoDelito::Byte: return 1;
    case TipoDatoDelito::Entero16: return 2;
    case TipoDatoDelito::Entero32: case TipoDatoDelito::Real32: return 4;
    case TipoDatoDelito::Entero64: return 8;
    default: return 0;
    }
}
bool offsetValido(uintptr_t offset, size_t n) {
    return offset != d::sinOffset && n && offset <= 0x100000 && n <= 0x100001 - offset;
}
bool direccion(uintptr_t base, uintptr_t offset, uintptr_t& destino) {
    if (!base || offset > std::numeric_limits<uintptr_t>::max() - base) return false;
    destino = base + offset;
    return true;
}
bool seSolapan(uintptr_t a, size_t na, uintptr_t b, size_t nb) {
    return na && nb && a < b + nb && b < a + na;
}
bool datosValidos() {
    if (globales.empty()) return false;
    for (const auto& t : d::tablas) {
        if (!t.nombre || !*t.nombre || !t.campos || !t.cantidadCampos ||
            t.cantidadCampos > d::maxCamposPorFila || !t.maxFilas || t.maxFilas > 65536 ||
            !offsetValido(t.offsetCantidad, sizeof(uint32_t)) ||
            !offsetValido(t.offsetFilas, sizeof(uintptr_t)) ||
            !offsetValido(t.offsetBloqueada, 1)) return false;
        const size_t idAncho = ancho(t.tipoIdentidad);
        if (t.tipoIdentidad != TipoDatoDelito::IndiceFila &&
            (t.tipoIdentidad == TipoDatoDelito::Real32 ||
             !offsetValido(t.offsetIdentidad, idAncho))) return false;
        if (t.firmaGlobal && *t.firmaGlobal &&
            (!t.longitudRip || t.longitudRip > 32 ||
             t.desplazamientoRip > t.longitudRip ||
             t.longitudRip - t.desplazamientoRip < sizeof(int32_t))) return false;
        for (size_t i = 0; i < t.cantidadCampos; ++i) {
            const auto& c = t.campos[i];
            const size_t n = ancho(c.tipo);
            if (!offsetValido(c.offset, n) ||
                (n < sizeof(c.valor) && c.valor >> (n * 8)) ||
                seSolapan(c.offset, n, t.offsetIdentidad, idAncho)) return false;
            if (c.tipo == TipoDatoDelito::Real32) {
                const uint32_t bits = static_cast<uint32_t>(c.valor);
                float valor{}; std::memcpy(&valor, &bits, sizeof(valor));
                if (!std::isfinite(valor)) return false;
            }
            for (size_t j = 0; j < i; ++j)
                if (seSolapan(c.offset, n, t.campos[j].offset, ancho(t.campos[j].tipo))) return false;
        }
    }
    return true;
}
bool vistaActual(size_t i, Vista& v) {
    const auto& t = d::tablas[i];
    uintptr_t tabla{}, filas{}, p{};
    uint32_t cantidad{};
    if (!leer(globales[i], tabla) || !tabla ||
        !direccion(tabla, t.offsetCantidad, p) || !leer(p, cantidad) ||
        !cantidad || cantidad > t.maxFilas ||
        !direccion(tabla, t.offsetFilas, p) || !leer(p, filas) || !filas ||
        cantidad > (std::numeric_limits<uintptr_t>::max() - filas) / sizeof(uintptr_t)) return false;
    v = {i, tabla, filas, cantidad};
    return true;
}
bool leerIdentidad(const TablaDelito& t, uintptr_t fila, uint32_t indice, uint64_t& id) {
    if (t.tipoIdentidad == TipoDatoDelito::IndiceFila) { id = indice; return true; }
    uintptr_t p{};
    id = 0;
    return direccion(fila, t.offsetIdentidad, p) && leer(p, &id, ancho(t.tipoIdentidad));
}
Vigencia vigente(const FilaGuardada& f) {
    const auto& t = d::tablas[f.vista.indice];
    uintptr_t actual{}, p{}, filas{};
    uint32_t cantidad{};
    if (!leer(globales[f.vista.indice], actual)) return Vigencia::Ilegible;
    if (actual != f.vista.tabla) return Vigencia::Cambio;
    if (!direccion(actual, t.offsetFilas, p) || !leer(p, filas) ||
        !direccion(actual, t.offsetCantidad, p) || !leer(p, cantidad)) return Vigencia::Ilegible;
    if (filas != f.vista.filas || cantidad != f.vista.cantidad || f.indice >= cantidad)
        return Vigencia::Cambio;
    if (!leer(filas + static_cast<uintptr_t>(f.indice) * sizeof(uintptr_t), actual)) return Vigencia::Ilegible;
    if (actual != f.fila) return Vigencia::Cambio;
    uint64_t id{};
    if (!leerIdentidad(t, actual, f.indice, id)) return Vigencia::Ilegible;
    return id == f.identidad ? Vigencia::Si : Vigencia::Cambio;
}
bool hayEscrituras() {
    for (const auto& f : copias) for (const auto& c : f.campos) if (c.escrito) return true;
    return false;
}
// Restaura únicamente nuestros valores que siguen en la misma fila viva. Un cambio
// externo se conserva. Una lectura/escritura temporalmente rechazada se reintenta.
bool restaurar(bool& conflicto) {
    for (auto& f : copias) {
        const auto v = vigente(f);
        if (v == Vigencia::Ilegible) continue;
        for (auto& c : f.campos) {
            if (!c.escrito) continue;
            if (v == Vigencia::Cambio) { c.escrito = false; continue; }
            uintptr_t p{};
            std::array<uint8_t, 8> actual{};
            const size_t n = ancho(c.dato.tipo);
            if (!direccion(f.fila, c.dato.offset, p) || !leer(p, actual.data(), n)) continue;
            if (std::memcmp(actual.data(), c.propio.data(), n) != 0) {
                c.escrito = false; conflicto = true; continue;
            }
            // Revalida justo antes de cada escritura, también durante rollback.
            if (vigente(f) != Vigencia::Si || !escribirDatos(p, c.anterior.data(), n)) continue;
            std::array<uint8_t, 8> leido{};
            if (leer(p, leido.data(), n) && std::memcmp(leido.data(), c.anterior.data(), n) == 0)
                c.escrito = false;
        }
    }
    const bool pendiente = hayEscrituras();
    if (!pendiente) copias.clear();
    aplicado.store(pendiente);
    return !pendiente;
}
bool capturar() {
    std::vector<FilaGuardada> nuevas;
    for (size_t i = 0; i < globales.size(); ++i) {
        const auto& t = d::tablas[i];
        Vista v{};
        if (!vistaActual(i, v)) return false;
        for (uint32_t j = 0; j < v.cantidad; ++j) {
            uintptr_t fila{}, p{};
            if (!leer(v.filas + static_cast<uintptr_t>(j) * sizeof(uintptr_t), fila)) return false;
            if (!fila) continue; // Las tablas publicadas admiten filas aún no cargadas.
            uint8_t bloqueada{};
            if (!direccion(fila, t.offsetBloqueada, p) || !leer(p, bloqueada) || bloqueada > 1) return false;
            if (bloqueada) continue;
            FilaGuardada f{};
            f.vista = v; f.indice = j; f.fila = fila;
            if (!leerIdentidad(t, fila, j, f.identidad)) return false;
            f.campos.reserve(t.cantidadCampos);
            for (size_t k = 0; k < t.cantidadCampos; ++k) {
                CampoGuardado c{}; c.dato = t.campos[k];
                const size_t n = ancho(c.dato.tipo);
                if (!direccion(fila, c.dato.offset, p) || !leer(p, c.anterior.data(), n)) return false;
                std::memcpy(c.propio.data(), &c.dato.valor, n);
                f.campos.push_back(c);
            }
            if (vigente(f) != Vigencia::Si) return false;
            nuevas.push_back(std::move(f));
        }
    }
    if (nuevas.empty()) return false;
    copias = std::move(nuevas); // Captura completa antes de la primera escritura.
    return true;
}
bool aplicar() {
    for (auto& f : copias) {
        if (vigente(f) != Vigencia::Si) return false;
        for (auto& c : f.campos) {
            uintptr_t p{};
            const size_t n = ancho(c.dato.tipo);
            std::array<uint8_t, 8> actual{};
            if (!direccion(f.fila, c.dato.offset, p) || !leer(p, actual.data(), n)) return false;
            const auto& esperado = c.escrito ? c.propio : c.anterior;
            if (std::memcmp(actual.data(), esperado.data(), n) != 0) return false;
            if (!c.escrito) {
                if (vigente(f) != Vigencia::Si || !escribirDatos(p, c.propio.data(), n)) return false;
                c.escrito = true; // También se restaura si falla la lectura de comprobación.
                if (!leer(p, actual.data(), n) || std::memcmp(actual.data(), c.propio.data(), n) != 0) return false;
            }
        }
    }
    aplicado.store(true);
    return true;
}
bool hayCambioDeTabla() {
    for (const auto& f : copias) if (vigente(f) == Vigencia::Cambio) return true;
    return false;
}
// Una fila cargada después de activar también debe recibir el tratamiento. Primero
// se restauran las filas antiguas aún vivas y después se vuelve a capturar el conjunto.
bool hayFilasNuevas() {
    size_t siguiente = 0;
    for (size_t i = 0; i < globales.size(); ++i) {
        Vista v{};
        if (!vistaActual(i, v)) return false;
        for (uint32_t j = 0; j < v.cantidad; ++j) {
            uintptr_t fila{}, p{}; uint8_t bloqueada{};
            if (!leer(v.filas + static_cast<uintptr_t>(j) * sizeof(uintptr_t), fila) || !fila) continue;
            if (!direccion(fila, d::tablas[i].offsetBloqueada, p) || !leer(p, bloqueada)) continue;
            if (bloqueada) continue;
            if (siguiente >= copias.size() || copias[siguiente].vista.indice != i ||
                copias[siguiente].indice != j || copias[siguiente].fila != fila) return true;
            ++siguiente;
        }
    }
    return siguiente != copias.size();
}
}

bool iniciarDelitos() {
    if (listo.load()) return true;
    if (!datosValidos()) { estado.store(Estado::SinDatos); return false; }
    globales.fill(0);
    for (size_t i = 0; i < globales.size(); ++i) {
        const auto& t = d::tablas[i];
        uintptr_t p{};
        if (t.firmaGlobal && *t.firmaGlobal) {
            const uintptr_t ancla = buscarUnico(t.firmaGlobal);
            if (ancla && direccion(ancla, t.instruccionRip, p))
                globales[i] = resolverRip(p, t.desplazamientoRip, t.longitudRip);
        } else globales[i] = resolverTablaPublica(t.nombre, t.nombreIndirecto);
        if (!globales[i]) { estado.store(Estado::SinTablas); return false; }
        // Dos nombres distintos no pueden terminar identificando una misma tabla.
        for (size_t j = 0; j < i; ++j) if (globales[j] == globales[i]) {
            estado.store(Estado::SinTablas); return false;
        }
    }
    rechazado.store(false); estado.store(Estado::Listo); listo.store(true);
    return true;
}
bool delitosDisponible() { return listo.load() && !rechazado.load(); }
void activarDelitos(bool activar) {
    solicitado.store(activar && delitosDisponible());
    if (!activar && aplicado.load()) estado.store(Estado::Restaurando);
}
bool delitosActivos() { return aplicado.load(); }
const char* estadoDelitos() {
    switch (estado.load()) {
    case Estado::SinDatos: return "Faltan metadatos de tablas/campos en datos_juego.hpp";
    case Estado::SinTablas: return "No se identificaron de forma única las tablas de delitos";
    case Estado::Listo: return "Tablas listas: recompensa y exenciones parciales; no bloquea todos los registros";
    case Estado::Esperando: return "Esperando personaje y tablas del mundo cargado";
    case Estado::Activo: return "Activo: precio WantedInfo y exenciones parciales; no impide todos los delitos";
    case Estado::Restaurando: return "Restaurando los valores originales en el hilo del juego";
    case Estado::RestauracionPendiente: return "Restauración pendiente: una página rechazó la escritura";
    case Estado::Conflicto: return "Desactivado: se conservó un cambio externo en las tablas";
    default: return "Desactivado: cambió una fila o se rechazó la escritura; revisar datos";
    }
}
void actualizarDelitos() {
    if (!listo.load() || procesando.test_and_set(std::memory_order_acquire)) return;
    struct Liberar { ~Liberar() { procesando.clear(std::memory_order_release); } } liberar;
    bool conflicto = false;
    if (!solicitado.load()) {
        if (!copias.empty()) {
            if (!restaurar(conflicto)) { estado.store(Estado::RestauracionPendiente); return; }
            if (conflicto) estado.store(Estado::Conflicto);
            else if (!rechazado.load()) estado.store(Estado::Listo);
        }
        return;
    }
    uintptr_t propietario{}, actor{};
    if (!resolverPersonajeControlado(propietario, actor) || !propietario || !actor) {
        aplicado.store(false); estado.store(Estado::Esperando); return;
    }
    if (!copias.empty() && (hayCambioDeTabla() || hayFilasNuevas())) {
        if (!restaurar(conflicto)) { aplicado.store(false); estado.store(Estado::RestauracionPendiente); return; }
        if (conflicto) {
            solicitado.store(false); rechazado.store(true); estado.store(Estado::Conflicto); return;
        }
    }
    if (copias.empty() && !capturar()) { aplicado.store(false); estado.store(Estado::Esperando); return; }
    if (!aplicar()) {
        solicitado.store(false); rechazado.store(true);
        const bool restaurado = restaurar(conflicto);
        estado.store(restaurado ? (conflicto ? Estado::Conflicto : Estado::Rechazado) : Estado::RestauracionPendiente);
        return;
    }
    estado.store(Estado::Activo);
}
}
