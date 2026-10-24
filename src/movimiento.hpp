#pragma once
namespace ds {
// El perfil nativo verifica el ejecutable y reconoce animaciones reales de planeo.
bool iniciarMovimiento();
bool vueloDisponible();
const char* estadoVuelo();
void activarVuelo(bool activar);
bool vueloActivo();
void ajustarVuelo(bool botonMantenido,float adelante,float vertical,
                  float multiplicadorAdelante,float velocidadVertical);
}
