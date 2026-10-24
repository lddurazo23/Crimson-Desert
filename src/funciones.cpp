#include "funciones.hpp"
#include "configuracion.hpp"
#include "vitales.hpp"
#include "mundo.hpp"
#include "movimiento.hpp"
#include "inventario.hpp"
#include "equipo.hpp"
#include "equipo_nativo.hpp"
#include "monturas_nivel5.hpp"
#include "clima.hpp"
#include "delitos.hpp"
#include "regiones.hpp"
#include "motor.hpp"

namespace ds {
std::atomic_bool listo{false};
std::atomic_bool falloInicio{false};
void notificarFalloInicio() {
    falloInicio.store(true,std::memory_order_release);
    listo.store(false,std::memory_order_release);
}
void aplicarPreferencias() {
    const auto& c=configuracion();
    for(size_t i=0;i<3;++i) activarVital(i,c.vitales[i]);
    activarMonturasNivel5(c.monturasNivel5);
    activarVuelo(c.vuelo);
    activarTiempo(c.tiempo);
    congelarTiempo(c.tiempo && c.congelado);
    fijarClima(c.ajustesDelClima);
    activarClima(c.clima);
    // Cada menú conserva su preferencia; ninguna acción persistente se repite al cargar.
    activarInventario(c.inventario && motorDisponible());
    activarEquipo(c.refinar && motorDisponible());
    activarEquipoNativo(c.equipoNuevo);
    activarDelitos(c.delitos && motorDisponible());
    activarConocimientos(c.conocimientos && motorDisponible());
}
void iniciarFunciones() {
    registrar("DesiertoSeguro 0.3: adaptadores públicos experimentales. Sin prueba dentro de Crimson Desert; opciones nuevas apagadas.");
    iniciarVitales();
    iniciarMundo();
    iniciarInventario();
    iniciarEquipo();
    iniciarMovimiento();
    iniciarMonturasNivel5();
    iniciarEquipoNativo();
    iniciarClima();
    iniciarDelitos();
    iniciarRegiones();
    iniciarMotor();
    listo.store(true,std::memory_order_release);
    for(size_t i=0;i<3;++i) registrar(estadoVital(i));
    registrar(estadoVuelo()); registrar(estadoMonturasNivel5());
    registrar(estadoEquipoNativo()); registrar(estadoTiempo());
    registrar(estadoInventario()); registrar(estadoEquipo()); registrar(estadoClima());
    registrar(estadoDelitos()); registrar(estadoRegiones()); registrar(estadoMotor());
}
}
