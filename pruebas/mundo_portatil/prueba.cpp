#include "mundo.hpp"
#include "datos_juego.hpp"
#include "MinHook.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
std::array<int,8> cliente{4,12,3,4},servidor{4,12,3,4};
std::array<unsigned char,0x400> motor{},sol{},otroSol{};
uintptr_t globalMotor{};
using Hook=void(*)(void*,float,float);Hook interceptado{};
float deltaRecibido{}; int llamadasOriginal{}; bool denegarSuperior=false;
void original(void*,float delta,float) {deltaRecibido=delta;++llamadasOriginal;}
MH_STATUS MH_Initialize(){return MH_OK;}
MH_STATUS MH_CreateHook(void*,void* hook,void** destino){interceptado=reinterpret_cast<Hook>(hook);*destino=reinterpret_cast<void*>(&original);return MH_OK;}
MH_STATUS MH_EnableHook(void*){return MH_OK;}
MH_STATUS MH_RemoveHook(void*){return MH_OK;}
namespace ds {
bool rango(uintptr_t p,size_t n) {for(auto [a,z]:{std::pair{reinterpret_cast<uintptr_t>(cliente.data()),sizeof(cliente)},std::pair{reinterpret_cast<uintptr_t>(servidor.data()),sizeof(servidor)},std::pair{reinterpret_cast<uintptr_t>(motor.data()),sizeof(motor)},std::pair{reinterpret_cast<uintptr_t>(sol.data()),sizeof(sol)},std::pair{reinterpret_cast<uintptr_t>(otroSol.data()),sizeof(otroSol)},std::pair{reinterpret_cast<uintptr_t>(&globalMotor),sizeof(globalMotor)}})if(p>=a && p+n<=a+z)return true;return false;}
bool leer(uintptr_t p,void* out,size_t n){if(!rango(p,n))return false;std::memcpy(out,reinterpret_cast<void*>(p),n);return true;}
bool escribirDatos(uintptr_t p,const void* in,size_t n){if(!rango(p,n) || (denegarSuperior && p==reinterpret_cast<uintptr_t>(sol.data())+datos::offsetLimiteSolSuperior))return false;std::memcpy(reinterpret_cast<void*>(p),in,n);return true;}
bool ejecutable(uintptr_t){return true;}
uintptr_t buscarUnico(const char* p){return std::strcmp(p,datos::firmaRelojes)==0?0x100000:std::strcmp(p,datos::firmaMotorSol)==0?0x200000:0x300000;}
uintptr_t resolverRip(uintptr_t p,uint32_t,uint32_t){if(p==0x100000+datos::instruccionRelojServidor)return reinterpret_cast<uintptr_t>(servidor.data());if(p==0x100000+datos::instruccionRelojCliente)return reinterpret_cast<uintptr_t>(cliente.data());return reinterpret_cast<uintptr_t>(&globalMotor);}
void procesarInventario(){}
}
void poner(std::array<unsigned char,0x400>& a,unsigned off,float v){std::memcpy(a.data()+off,&v,sizeof(v));}
float sacar(const std::array<unsigned char,0x400>& a,unsigned off){float v;std::memcpy(&v,a.data()+off,sizeof(v));return v;}
void tick(){interceptado(nullptr,0.25f,1.f);}
int main(){
 using namespace ds;
 globalMotor=reinterpret_cast<uintptr_t>(motor.data());auto ptrSol=reinterpret_cast<uintptr_t>(sol.data());std::memcpy(motor.data()+datos::offsetAdministradorSol,&ptrSol,sizeof(ptrSol));
 for(auto* a:{&sol,&otroSol}) {poner(*a,datos::offsetHoraSol,12);poner(*a,datos::offsetLimiteSolInferior,0);poner(*a,datos::offsetLimiteSolSuperior,24);}
 assert(iniciarMundo());assert(tiempoDisponible());tick();assert(deltaRecibido==0.25f);assert(!tiempoActivo());
 activarTiempo(true);congelarTiempo(true);tick();assert(deltaRecibido==0.f);assert(sacar(sol,datos::offsetLimiteSolInferior)==12);assert(sacar(sol,datos::offsetLimiteSolSuperior)==12);
 fijarHora(18,30);tick();assert(cliente[1]==18&&cliente[2]==30&&servidor[1]==18&&servidor[2]==30);assert(sacar(sol,datos::offsetLimiteSolInferior)==18.5f);
 activarTiempo(false);tick();assert(sacar(sol,datos::offsetLimiteSolInferior)==0);assert(sacar(sol,datos::offsetLimiteSolSuperior)==24);assert(deltaRecibido==0.25f);
 activarTiempo(true);tick();auto oldLow=sacar(sol,datos::offsetLimiteSolInferior);ptrSol=reinterpret_cast<uintptr_t>(otroSol.data());std::memcpy(motor.data()+datos::offsetAdministradorSol,&ptrSol,sizeof(ptrSol));tick();activarTiempo(false);tick();assert(sacar(sol,datos::offsetLimiteSolInferior)==oldLow);assert(sacar(otroSol,datos::offsetLimiteSolInferior)==0);assert(sacar(otroSol,datos::offsetLimiteSolSuperior)==24);
 ptrSol=reinterpret_cast<uintptr_t>(sol.data());std::memcpy(motor.data()+datos::offsetAdministradorSol,&ptrSol,sizeof(ptrSol));poner(sol,datos::offsetLimiteSolInferior,0);poner(sol,datos::offsetLimiteSolSuperior,24);
 activarTiempo(true);tick();denegarSuperior=true;fijarHora(19,0);tick();assert(!tiempoActivo());denegarSuperior=false;tick();assert(sacar(sol,datos::offsetLimiteSolInferior)==0);assert(sacar(sol,datos::offsetLimiteSolSuperior)==24);
 assert(!tiempoDisponible());activarTiempo(true);assert(!tiempoActivo());assert(llamadasOriginal==10);std::cout<<"Pruebas simuladas de reloj aprobadas\n";
}
