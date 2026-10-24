#pragma once

namespace ds {
struct AjustesClima {
    float precipitacion = 0.0f;
    float nubosidad = 0.0f;
    float humedad = 0.0f;
    float viento = 0.0f;
    float niebla = 0.0f;
    bool nieve = false;
};
// El menú solo publica valores; los dos productores nativos aplican el cambio.
bool iniciarClima();
bool climaDisponible();
const char* estadoClima();
void activarClima(bool activar);
// Incluye una solicitud guardada que aún espera el primer campo válido.
bool climaActivo();
AjustesClima ajustesClima();
void fijarClima(const AjustesClima& ajustes);
// Actualiza únicamente el vencimiento del estado mostrado, sin escribir memoria.
void actualizarClima();
}
