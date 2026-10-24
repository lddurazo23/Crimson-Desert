#pragma once
#include <cstddef>
#include <cstdint>

namespace ds {
// Lecturas protegidas contra páginas retiradas durante una transición del juego.
bool leer(uintptr_t direccion, void* destino, size_t cantidad);
// Solo escribe datos ya modificables; nunca cambia permisos ni parchea instrucciones.
bool escribirDatos(uintptr_t direccion, const void* origen, size_t cantidad);
bool ejecutable(uintptr_t direccion);
// Usa límites reales de .pdata/unwind de Windows, nunca una ventana inferida.
bool testigoEnFuncion(uintptr_t funcion, const char* firmaTestigo);
uintptr_t buscarUnico(const char* patron);
uintptr_t resolverRip(uintptr_t instruccion, uint32_t desplazamiento32, uint32_t longitud);
template<class T> bool leer(uintptr_t direccion, T& destino) {
    return leer(direccion, &destino, sizeof(T));
}
template<class T> bool escribir(uintptr_t direccion, const T& origen) {
    return escribirDatos(direccion, &origen, sizeof(T));
}
}
