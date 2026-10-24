#pragma once
#include <cstdint>
extern void* direccionRetornoSimulada;
extern uintptr_t direccionTlsSimulada;
inline void* _ReturnAddress() { return direccionRetornoSimulada; }
inline uint64_t __readgsqword(unsigned) { return direccionTlsSimulada; }
