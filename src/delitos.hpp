#pragma once
#include <cstddef>
#include <cstdint>

namespace ds {
// Metadatos consumidos por el motor. IndiceFila solo identifica filas: nunca se escribe.
enum class TipoDatoDelito : uint8_t { Byte, Entero16, Entero32, Entero64, Real32, IndiceFila };
struct CampoDelito {
    uintptr_t offset;
    TipoDatoDelito tipo;
    uint64_t valor; // Representación binaria del valor, incluido Real32.
};
struct TablaDelito {
    const char* nombre;
    bool nombreIndirecto;
    const char* firmaGlobal; // Alternativa AOB única, resuelta por RIP; vacía usa el nombre.
    uint32_t instruccionRip, desplazamientoRip, longitudRip;
    uintptr_t offsetCantidad, offsetFilas, offsetIdentidad;
    TipoDatoDelito tipoIdentidad;
    uintptr_t offsetBloqueada;
    uint32_t maxFilas;
    const CampoDelito* campos;
    size_t cantidadCampos;
};
// Solicitudes desde el menú; las escrituras solo ocurren en actualizarDelitos(),
// llamado por el punto de ejecución del juego junto al resto de operaciones.
bool iniciarDelitos();
bool delitosDisponible();
const char* estadoDelitos();
void activarDelitos(bool activar);
bool delitosActivos();
void actualizarDelitos();
}
