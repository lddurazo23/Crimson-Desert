#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ds {
struct TipoEquipoNativo {
    uint32_t claveExterna = 0;
    std::string nombre;
};
// Alternativa explícita: crea UNA pieza nueva, no modifica objetos existentes.
// El perfil nativo completo pertenece exclusivamente al ejecutable 1.0.0.2976.
bool iniciarEquipoNativo();
bool equipoNativoDisponible();
bool equipoNativoActivo();
void activarEquipoNativo(bool activo);
bool solicitarEquipoNativo(uint32_t claveExterna);
std::vector<TipoEquipoNativo> catalogoEquipoNativo();
const char* estadoEquipoNativo();
}
