#include "movimiento.hpp"
#include "planeo_nativo.hpp"

namespace ds {
bool iniciarMovimiento(){return iniciarPlaneoNativo();}
bool vueloDisponible(){return planeoNativoDisponible();}
const char* estadoVuelo(){return estadoPlaneoNativo();}
void activarVuelo(bool activar){activarPlaneoNativo(activar);}
bool vueloActivo(){return planeoNativoActivo();}
void ajustarVuelo(bool mantenido,float adelante,float vertical,float factor,float rapidez){
    controlesPlaneoNativo(mantenido,adelante,vertical,factor,rapidez);
}
}
