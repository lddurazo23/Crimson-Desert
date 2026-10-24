#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ds {
struct ItemInventario {
    uint64_t idInstancia{};
    uint16_t tipoId{};
    std::string nombre;
    std::string almacen;
    std::string categoria;
    int64_t cantidad{};
    int64_t cantidadMaxima{};
    bool editable{};
};
struct TipoInventario {
    uint16_t tipoId{};
    std::string nombre;
    std::string categoria;
    int64_t cantidadMaxima{};
};
bool iniciarInventario();
bool inventarioDisponible();
bool agregarInventarioDisponible();
bool disenoInventarioLegadoVerificado();
const char* estadoInventario();
bool activarInventario(bool activar);
bool inventarioActivo();
// Sólo devuelve una copia tomada en el hilo del motor; nunca sigue punteros desde el menú.
std::vector<ItemInventario> leerInventario();
std::vector<TipoInventario> leerCatalogoInventario();
bool solicitarCantidad(uint64_t idInstancia, int64_t cantidad);
bool solicitarAgregar(uint16_t tipoId, int64_t cantidad);
bool solicitarEliminar(uint64_t idInstancia);
// Llamar únicamente desde la cola posterior al tick MoveUpdate del juego.
void procesarInventario();
void falloParcialInventario();
}
