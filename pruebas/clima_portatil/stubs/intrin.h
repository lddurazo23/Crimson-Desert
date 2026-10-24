#pragma once
extern void* direccionRetornoSimulada;
inline void* _ReturnAddress() { return direccionRetornoSimulada; }
