#pragma once
namespace ds {
// Perfil nativo completo: no usa campos hipotéticos para reconocer planeo.
bool iniciarPlaneoNativo();
bool planeoNativoDisponible();
const char* estadoPlaneoNativo();
void activarPlaneoNativo(bool activar);
bool planeoNativoActivo();
void controlesPlaneoNativo(bool mantenido,float adelante,float vertical,
                         float multiplicador,float rapidezVertical);
}
