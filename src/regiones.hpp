#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ds {
// Alternativa nativa comprobada: aprender entradas individuales del catálogo.
// Un lugar descubierto no acredita niebla completa ni habilita una puerta.
enum class ClaseDescubrimiento : uint8_t { Lugar, Abismo };
struct Descubrimiento {
    uint32_t clave = 0;
    std::string nombreInterno;
    ClaseDescubrimiento clase = ClaseDescubrimiento::Lugar;
    bool aprendido = false;
};
bool iniciarRegiones();
size_t regionCantidad();
const char* nombreRegion(size_t region);
bool descubrimientosDisponible();
void activarConocimientos(bool activo);
bool conocimientosActivos();
std::vector<Descubrimiento> leerDescubrimientos();
bool solicitarAprenderClave(uint32_t clave);
const char* estadoRegiones();
// Sólo el tick publicado del juego aplica solicitudes; el menú no escribe.
void procesarRegiones();
}
