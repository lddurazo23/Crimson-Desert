#pragma once
#include <cstddef>
#include <cstdint>

namespace ds {
// Índices: 0 salud, 1 stamina, 2 espíritu. La instalación nunca activa opciones.
bool iniciarVitales();
void activarVital(size_t indice, bool activo);
bool vitalDisponible(size_t indice);
bool vitalActivo(size_t indice);
const char* estadoVital(size_t indice);
// Identidad compartida para funciones que necesitan comprobar el cuerpo controlado.
bool iniciarResolucionPersonaje();
bool resolverPersonajeControlado(uintptr_t& propietario, uintptr_t& actor);
uintptr_t propietarioControlado();
uintptr_t actorControlado();
}
