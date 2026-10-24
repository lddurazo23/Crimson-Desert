#pragma once

namespace ds {
// Las solicitudes se encolan desde el menú; el hook del reloj las aplica en el hilo del juego.
bool iniciarMundo();
bool tiempoDisponible();
const char* estadoTiempo();
void activarTiempo(bool activar);
bool tiempoActivo();
void fijarHora(int hora, int minuto);
void congelarTiempo(bool congelar);
}
