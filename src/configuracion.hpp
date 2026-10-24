#pragma once
#include <array>
#include "clima.hpp"
#include <filesystem>
#include <string>
#include <map>
#include <cstdint>

namespace ds {
struct Configuracion {
    int teclaMenu = 0x24;
    int teclaVuelo = 0x75;
    int teclaSubir = 0x20;
    int teclaBajar = 0x11;
    int botonVuelo = 0x80;
    float vueloAdelante = 2.0f;
    float vueloVertical = 15.0f;
    std::array<bool, 3> vitales{};
    bool vuelo = false, inventario = false, tiempo = false, congelado = false;
    bool monturasNivel5=false, delitos=false, refinar=false, equipoNuevo=false;
    bool clima=false, conocimientos=false;
    int regionMapa=0;
    AjustesClima ajustesDelClima{};
};
Configuracion& configuracion();
bool cargarConfiguracion();
bool guardarConfiguracion();
std::filesystem::path carpetaDatos();
void registrar(const char* texto);
const char* estadoGuardado();
}
