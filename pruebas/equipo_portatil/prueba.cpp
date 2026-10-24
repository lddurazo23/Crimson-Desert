#include "equipo.cpp"
#include <cassert>
#include <iostream>
namespace {
struct Region { uintptr_t p; size_t n; };
std::vector<Region> regiones;
size_t escrituras = 0;
bool layoutValido = true;
template<size_t N> void registrar(std::array<uint8_t,N>& a) { regiones.push_back({reinterpret_cast<uintptr_t>(a.data()),a.size()}); }
bool rango(uintptr_t p,size_t n) {
    return std::any_of(regiones.begin(),regiones.end(),[&](const Region& r){return p>=r.p && p-r.p<=r.n && n<=r.n-(p-r.p);});
}
template<class T,size_t N> void poner(std::array<uint8_t,N>& a,size_t off,T v) { std::memcpy(a.data()+off,&v,sizeof(v)); }
}
namespace ds {
bool disenoInventarioLegadoVerificado() { return layoutValido; }
bool leer(uintptr_t p,void* dst,size_t n) { if(!rango(p,n)) return false; std::memcpy(dst,reinterpret_cast<void*>(p),n); return true; }
bool escribirDatos(uintptr_t p,const void* src,size_t n) { if(!rango(p,n)) return false; ++escrituras; std::memcpy(reinterpret_cast<void*>(p),src,n); return true; }
bool ejecutable(uintptr_t) { return true; }
uintptr_t buscarUnico(const char*) { return 0; }
}
namespace trinity::game {
uintptr_t Inventory::ClientCharacterAddr() { return 0; }
uintptr_t Inventory::ServerCharacterAddr() { return 0; }
}
int main() {
    using namespace ds;
    std::array<uint8_t,0xC0> cliente{},servidor{};
    registrar(cliente); registrar(servidor);
    for(auto* item:{&cliente,&servidor}) { poner(*item,0,uint64_t(1234)); poner(*item,8,uint16_t(15)); poner(*item,10,uint16_t(2)); }
    ParEquipo par{reinterpret_cast<uintptr_t>(cliente.data()),reinterpret_cast<uintptr_t>(servidor.data()),1234,15,true,true,7};
    std::vector<Cambio> cambios;
    assert(planificarRefinar(cambios,par) && cambios.size()==2 && escrituras==0);
    for(const auto& c:cambios) assert(escribirCambio(c,true));
    uint16_t nivel=0; assert(campo(par.cliente,10,nivel)&&nivel==7);
    assert(restaurar(cambios,cambios.size()));
    assert(campo(par.autoridad,10,nivel)&&nivel==2);
    cambios.clear(); par.refinamientoMaximo=0;
    assert(!planificarRefinar(cambios,par) && cambios.empty());
    par.refinamientoMaximo=11;
    assert(!planificarRefinar(cambios,par) && cambios.empty());
    par.refinamientoMaximo=7;
    poner(servidor,10,uint16_t(3));
    assert(!planificarRefinar(cambios,par) && cambios.empty());
    poner(servidor,10,uint16_t(2));
    poner(servidor,0,uint64_t(999)); assert(!identidad(par)); poner(servidor,0,uint64_t(1234));
    pendiente.store(Accion::Refinar); cancelarSolicitudesEquipo();
    assert(pendiente.load()==Accion::Ninguna);
    instalado.store(true);
    assert(refinarDisponible());
    layoutValido=false;
    assert(!refinarDisponible());
    assert(!activarEquipo(true));
    std::cout<<"Correcto: límite por tipo, mirrors, planificación sin escrituras, rollback, cancelación y rechazo de layout C0 no verificado.\n";
}
