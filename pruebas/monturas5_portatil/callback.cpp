#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include "monturas_nivel5.cpp"
extern "C" void DsMonturasGateway() {}
extern "C" uint64_t DsMonturasLeerXcr0() {return 3;}
MH_STATUS MH_Initialize(){return MH_OK;}
MH_STATUS MH_CreateHook(void*,void*,void** p){*p=nullptr;return MH_ERROR_UNKNOWN;}
MH_STATUS MH_EnableHook(void*){return MH_ERROR_UNKNOWN;}
MH_STATUS MH_DisableHook(void*){return MH_OK;}
MH_STATUS MH_RemoveHook(void*){return MH_OK;}
namespace {
alignas(4096) std::array<unsigned char,4096> memoria{};
template<class T>void poner(size_t offset,T valor){std::memcpy(memoria.data()+offset,&valor,sizeof(valor));}
}
namespace ds {
bool leer(uintptr_t p,void* destino,size_t n){
    const auto base=reinterpret_cast<uintptr_t>(memoria.data());
    if(p<base || p-base>memoria.size() || n>memoria.size()-(p-base))return false;
    std::memcpy(destino,reinterpret_cast<void*>(p),n);return true;
}
uintptr_t buscarUnico(const char*){return 0;}
bool ejecutable(uintptr_t){return false;}
}
int main(){
    using namespace ds;namespace d=datos::monturas5;
    const auto base=reinterpret_cast<uintptr_t>(memoria.data());
    const size_t campo=d::distanciaCampo;
    const auto preparar=[&](){
        memoria.fill(0);poner(campo+d::actual,uint64_t(1000));poner(campo+d::maximo,d::maximoVelocidad);
        poner(campo+d::minimo,d::minimoVelocidad);poner(campo+d::paso,d::pasoVelocidad);
        poner(campo+d::estadistica,d::tipoEstadistica);poner(campo+d::subtipo,d::subtipoVelocidad);poner(campo+d::ordinal,d::ordinalVelocidad);
        instalado=true;activo=true;fallo=false;
    };
    ContextoMonturas5 c{};c.r14=base;c.r12=base+campo;c.rdx=123;
    preparar();DsMonturasCallback(&c);assert(c.rdx==5000);uint64_t actual=0;assert(leer(base+campo,actual)&&actual==5000);
    preparar();activo=false;c.rdx=123;DsMonturasCallback(&c);assert(c.rdx==123);assert(leer(base+campo,actual)&&actual==1000);
    preparar();poner(campo+d::subtipo,uint8_t(14));c.rdx=123;DsMonturasCallback(&c);assert(c.rdx==123);
    preparar();poner(campo+d::ordinal,uint16_t(48));DsMonturasCallback(&c);assert(c.rdx==123);
    preparar();poner(campo+d::actual,uint64_t(5001));DsMonturasCallback(&c);assert(c.rdx==123);
    preparar();c.r14=base+1;DsMonturasCallback(&c);assert(c.rdx==123);c.r14=base;
    preparar();c.r12=base+campo+1;DsMonturasCallback(&c);assert(c.rdx==123);c.r12=base+campo;
    preparar();c.r12=0;DsMonturasCallback(&c);assert(c.rdx==123);
    std::cout<<"Monturas: callback real valida selector, límites, alineación, direcciones y desactivación.\n";
}
