#pragma once
#include <atomic>
namespace ds {
extern std::atomic_bool listo;
extern std::atomic_bool falloInicio;
void notificarFalloInicio();
void iniciarFunciones();
void aplicarPreferencias();
}
