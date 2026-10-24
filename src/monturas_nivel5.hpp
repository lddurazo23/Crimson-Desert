#pragma once
namespace ds {
// Alternativa global basada en el campo de nivel publicado para 2.03.02.
bool iniciarMonturasNivel5();
bool monturasNivel5Disponible();
bool monturasNivel5Activas();
void activarMonturasNivel5(bool activar);
const char* estadoMonturasNivel5();
}
