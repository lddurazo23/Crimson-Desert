#pragma once
#include <cstddef>
#include <cstdint>

namespace ds {
// Direcciones efímeras: sólo se entregan y consumen dentro del tick del motor.
struct ParEquipo {
    uintptr_t cliente = 0, autoridad = 0;
    uint64_t instancia = 0;
    uint16_t tipo = 0;
    bool refinable = false, admiteRanuras = false;
    uint16_t refinamientoMaximo = 0;
};
// El límite sale de la curva REAL de ItemInfo: min(10, enchantCount-1).
// Cero excluye el objeto; +0xA también se usa como subtipo en otros objetos.
using ClasificarEquipo = bool (*)(uint16_t tipo, bool* refinable, bool* admiteRanuras,
                                 uint16_t* refinamientoMaximo);
bool iniciarEquipo();
bool activarEquipo(bool valor);
bool equipoActivo();
bool refinarDisponible();
bool solicitarRefinarTodo();
void cancelarSolicitudesEquipo();
const char* estadoEquipo();
// Llamar desde el mismo pump seguro que procesarInventario. Incluye los pares
// de mochila; el módulo añade el equipo vestido desde los dos actores.
void procesarEquipo(const ParEquipo* mochila, size_t cantidad, ClasificarEquipo clasificar);
}
