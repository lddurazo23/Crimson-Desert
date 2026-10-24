// Datos sintéticos de prueba; ningún ID, nombre ni dirección de aquí pertenece al juego.
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
#include "regiones.cpp"

namespace {
using B=std::array<uint8_t,1024>;
B modulo{},dueno{},sub{},componente{},estados{},tabla{},def0{},def1{},def2{},teb{},tls{};
std::array<uintptr_t,3> defs{};
uintptr_t globalTabla=0,arregloTls=0;
std::array<uintptr_t,3> nombres{};
char nombre0[]="Knowledge_Node_Prueba";
char nombre1[]="Knowledge_AbyssRuins_Prueba";
char nombre2[]="Knowledge_Skill_Prueba";
std::vector<std::pair<uintptr_t,size_t>> regiones;
uintptr_t seleccionado=0,denegado=0;
ULONGLONG tiempo=1000;
unsigned llamadas=0;
bool confirmar=true,excepcion=false,rechazar=false,falloRestaurar=false;
template<class T>void poner(uintptr_t p,const T& valor){std::memcpy(reinterpret_cast<void*>(p),&valor,sizeof(valor));}
template<class T>void poner(B& b,size_t off,const T& valor){poner(reinterpret_cast<uintptr_t>(b.data())+off,valor);}
template<class T>T sacar(const B& b,size_t off){T valor{};std::memcpy(&valor,b.data()+off,sizeof(valor));return valor;}
template<class T>void agregar(T& x){regiones.emplace_back(reinterpret_cast<uintptr_t>(&x),sizeof(x));}
bool rango(uintptr_t p,size_t n){for(auto[a,z]:regiones)if(p>=a&&p-a<=z&&n<=z-(p-a))return true;return false;}
LONG consulta(HANDLE,ULONG,PVOID info,ULONG largo,PULONG){
    assert(largo==ds::datos::regiones::tamanoInformacionHilo);
    const auto addr=reinterpret_cast<uintptr_t>(teb.data());
    std::memcpy(static_cast<uint8_t*>(info)+ds::datos::regiones::informacionTeb,&addr,sizeof(addr));return 0;
}
uint32_t* nativa(uintptr_t comp,uint32_t* error,const uint16_t* fila,uint32_t nivel,uint32_t cero,uint8_t fuente,void* extra){
    assert(comp==reinterpret_cast<uintptr_t>(componente.data()));assert(nivel==1&&cero==0&&extra==nullptr);
    assert(fuente==ds::datos::regiones::fuenteOtorgamiento);
    assert(tls[ds::datos::regiones::campoRealm]==1);++llamadas;
    if(excepcion)throw std::runtime_error("Excepción nativa sintética");
    if(falloRestaurar)denegado=reinterpret_cast<uintptr_t>(tls.data())+ds::datos::regiones::campoRealm;
    *error=rechazar?99:0;
    if(confirmar&&!rechazar)estados[*fila*ds::datos::regiones::pasoEstadoConocimiento]=1;
    return error;
}
void preparar(){
    using namespace ds;
    trabajo={};ultimaLectura=0;revision=0;instalado=true;solicitado=false;bloqueado=false;procesando.clear();
    limpiarCola();regiones.clear();tiempo=1000;llamadas=0;confirmar=true;excepcion=false;rechazar=false;falloRestaurar=false;denegado=0;
    for(auto*b:{&modulo,&dueno,&sub,&componente,&estados,&tabla,&def0,&def1,&def2,&teb,&tls}){b->fill(0);agregar(*b);}
    agregar(defs);agregar(nombres);agregar(nombre0);agregar(nombre1);agregar(nombre2);agregar(globalTabla);agregar(arregloTls);
    seleccionado=reinterpret_cast<uintptr_t>(dueno.data());
    poner(dueno,datos::regiones::propietarioComponentes,reinterpret_cast<uintptr_t>(sub.data()));
    poner(sub,datos::regiones::componenteConocimiento,reinterpret_cast<uintptr_t>(componente.data()));
    const auto vt=reinterpret_cast<uintptr_t>(modulo.data())+64;
    const auto col=reinterpret_cast<uintptr_t>(modulo.data())+128;
    poner(componente,0,vt);poner(vt-8,col);poner(col+0x0C,uint32_t{256});
    std::strcpy(reinterpret_cast<char*>(modulo.data()+256+0x10),datos::regiones::claseConocimiento);
    poner(componente,datos::regiones::conocimientoCantidad,uint32_t{3});
    poner(componente,datos::regiones::conocimientoEstados,reinterpret_cast<uintptr_t>(estados.data()));
    globalTabla=reinterpret_cast<uintptr_t>(tabla.data());
    poner(tabla,datos::regiones::tablaCantidad,uint32_t{3});poner(tabla,datos::regiones::tablaFilas,reinterpret_cast<uintptr_t>(defs.data()));
    B* filas[]={&def0,&def1,&def2};char* textos[]={nombre0,nombre1,nombre2};
    for(size_t i=0;i<3;++i){defs[i]=reinterpret_cast<uintptr_t>(filas[i]->data());nombres[i]=reinterpret_cast<uintptr_t>(textos[i]);
        poner(*filas[i],datos::regiones::filaClave,uint32_t(100+i));poner(*filas[i],datos::regiones::filaNombre,reinterpret_cast<uintptr_t>(&nombres[i]));}
    arregloTls=reinterpret_cast<uintptr_t>(tls.data());
    poner(teb,datos::regiones::tebPunterosTls,reinterpret_cast<uintptr_t>(&arregloTls));
    globalConocimiento=reinterpret_cast<uintptr_t>(&globalTabla);otorgar=nativa;consultarHilo=consulta;
    activarConocimientos(true);procesarRegiones();
}
}
ULONGLONG GetTickCount64(){return tiempo;}
HMODULE GetModuleHandleW(const wchar_t* nombre){return nombre?reinterpret_cast<void*>(1):modulo.data();}
HANDLE GetCurrentThread(){return reinterpret_cast<void*>(2);}
FARPROC GetProcAddress(HMODULE,const char*){return reinterpret_cast<FARPROC>(&consulta);}
namespace ds {
bool leer(uintptr_t p,void* salida,size_t n){if(!rango(p,n))return false;std::memcpy(salida,reinterpret_cast<const void*>(p),n);return true;}
bool escribirDatos(uintptr_t p,const void* entrada,size_t n){if(p==denegado||!rango(p,n))return false;std::memcpy(reinterpret_cast<void*>(p),entrada,n);return true;}
uintptr_t buscarUnico(const char*){return 0;}
uintptr_t resolverRip(uintptr_t,uint32_t,uint32_t){return 0;}
bool ejecutable(uintptr_t){return false;}
uintptr_t resolverTablaPublica(const char*,bool){return reinterpret_cast<uintptr_t>(&globalTabla);}
bool iniciarResolucionPersonaje(){return true;}
uintptr_t propietarioControlado(){return seleccionado;}
}
int main(){
    using namespace ds;
    ClaseDescubrimiento c{};
    assert(categoria("Knowledge_Node_Prueba",c)&&c==ClaseDescubrimiento::Lugar);
    assert(categoria("Knowledge_AbyssRuins_Prueba",c)&&c==ClaseDescubrimiento::Abismo);
    assert(!categoria("Knowledge_NodeExtra_Prueba",c));assert(!categoria("Knowledge_Skill_Prueba",c));
    preparar();auto items=leerDescubrimientos();assert(items.size()==2);assert(items[0].clave==100&&!items[0].aprendido);
    assert(!solicitarAprenderClave(102));assert(solicitarAprenderClave(100));assert(!solicitarAprenderClave(100));
    procesarRegiones();assert(llamadas==1&&trabajo.activo&&tls[datos::regiones::campoRealm]==0);
    procesarRegiones();assert(!trabajo.activo);items=leerDescubrimientos();assert(items[0].aprendido);assert(!solicitarAprenderClave(100));
    preparar();confirmar=false;assert(solicitarAprenderClave(100));procesarRegiones();procesarRegiones();assert(llamadas==1);
    tiempo+=6000;procesarRegiones();assert(!trabajo.activo&&llamadas==1);
    preparar();assert(solicitarAprenderClave(100));activarConocimientos(false);activarConocimientos(true);procesarRegiones();assert(llamadas==0);
    preparar();confirmar=false;assert(solicitarAprenderClave(100));procesarRegiones();seleccionado=0;procesarRegiones();assert(!trabajo.activo&&leerDescubrimientos().empty());
    preparar();excepcion=true;assert(solicitarAprenderClave(100));procesarRegiones();assert(llamadas==1&&tls[datos::regiones::campoRealm]==0&&!descubrimientosDisponible());
    preparar();rechazar=true;assert(solicitarAprenderClave(100));procesarRegiones();assert(llamadas==1&&tls[datos::regiones::campoRealm]==0&&!trabajo.activo&&descubrimientosDisponible());
    preparar();falloRestaurar=true;assert(solicitarAprenderClave(100));procesarRegiones();assert(!descubrimientosDisponible());denegado=0;
    preparar();poner(def1,datos::regiones::filaClave,uint32_t{100});ultimaLectura=0;procesarRegiones();assert(llamadas==0);
    instalado=false;assert(!iniciarRegiones());assert(!descubrimientosDisponible());
    std::cout<<"Descubrimientos: catálogo, categorías, autoridad, confirmación, cancelación, identidad y rechazo aprobados.\n";
}
