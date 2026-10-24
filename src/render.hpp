#pragma once

// El renderizador no depende de ninguna dirección ni estructura del juego.
namespace ds {
using DibujarMenu = void (*)();
using AvisarRender = void (*)(const char*);
using ConsultarMenu = bool (*)();

// Debe llamarse una sola vez desde un hilo fuera de DllMain.
// La DLL permanece cargada hasta que termine el proceso; no permite descarga en vivo.
bool iniciarRender(DibujarMenu dibujar, AvisarRender avisar, ConsultarMenu visible);
bool renderDisponible();
}
