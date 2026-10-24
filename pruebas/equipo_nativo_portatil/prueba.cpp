// Datos sintéticos locales: ninguna dirección de esta fixture es un dato del juego.
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <climits>
#include "equipo_nativo.cpp"

uint64_t tiempoSimulado=10000;
uintptr_t baseSimulada=0,direccionTlsSimulada=0;
void* direccionRetornoSimulada=nullptr;
namespace {
using namespace ds;
namespace n=ds::datos::equipoNativo;
std::array<uint8_t,0x100> raiz{},administrador{},metadata{};
std::array<uint8_t,0x200> cliente{},autoridad{},componentes{};
std::array<uint8_t,0x100> controlCliente{},controlServidor{};
std::array<uint8_t,0x300> localTls{};
std::array<uint8_t,0x100> holder{},cubeta{};
std::array<uint8_t,n::pasoItem*4> slots{};
std::array<uint8_t,0x500> def{};
std::array<uint8_t,30> gemas{};
uintptr_t rootGlobal=0,tlsGlobal=0;
std::array<uintptr_t,1> cubetas{};
unsigned construidos=0,destruidos=0,convertidos=0,otorgados=0,tareasOriginales=0;
unsigned variante=0;
uint32_t identidadPrueba=0xA0000001,clavePrueba=0;
uint16_t tipoPrueba=123;
int error=0; uintptr_t contexto=0;
void* argumentoTrabajo=nullptr;
int* argumentoError=nullptr;
void* argumentoContexto=nullptr;
uintptr_t denegar=0;

template<class T> void poner(uintptr_t p,T valor){std::memcpy(reinterpret_cast<void*>(p),&valor,sizeof(valor));}
template<class T,size_t N> void poner(std::array<uint8_t,N>& a,size_t off,T valor){assert(off+sizeof(T)<=N);poner(reinterpret_cast<uintptr_t>(a.data())+off,valor);}
template<class T> T tomar(uintptr_t p){T valor{};std::memcpy(&valor,reinterpret_cast<void*>(p),sizeof(valor));return valor;}
template<size_t N> uintptr_t ptr(std::array<uint8_t,N>& a){return reinterpret_cast<uintptr_t>(a.data());}
bool lookup(uint16_t* tipo,uint32_t clave){*tipo=tipoPrueba;return variante!=8 && clave==clavePrueba;}
unsigned char* definition(const uint16_t*){return def.data();}
void* construct(void* destino,const uint16_t* tipo,int64_t cuantos){
    ++construidos;if(variante==9)throw std::runtime_error("fallo sintético de constructor");
    std::memset(destino,0,n::pasoItem);auto p=reinterpret_cast<uintptr_t>(destino);
    poner(p+n::itemUid,UINT64_MAX);poner(p+n::itemTipo,*tipo);poner(p+n::itemCantidad,cuantos);
    poner(p+n::itemRanuras,ptr(gemas));poner(p+n::itemCantidadRanuras,uint32_t(variante==1?4:5));
    poner(p+n::itemCapacidadRanuras,uint32_t(5));poner(p+n::itemAbiertas,uint8_t(0));
    gemas.fill(0);for(size_t i=0;i<5;++i){poner(gemas,i*6,uint16_t(0xffff));poner(gemas,i*6+4,uint8_t(0xff));}
    if(variante==2) poner(gemas,0,uint16_t(42));
    return destino;
}
void destroy(void*){++destruidos;}
void* convert(void* destino,const void* origen,uint16_t slot){
    ++convertidos;assert(slot==0xffff);auto a=reinterpret_cast<uintptr_t>(origen),b=reinterpret_cast<uintptr_t>(destino);
    poner(b+n::initUid,tomar<uint64_t>(a+n::itemUid));poner(b+n::initClave,clavePrueba);
    poner(b+n::initCantidad,tomar<int64_t>(a+n::itemCantidad));
    std::memcpy(reinterpret_cast<void*>(b+n::initRanuras),gemas.data(),gemas.size());
    poner(b+n::initAbiertas,uint8_t(variante==3?0:tomar<uint8_t>(a+n::itemAbiertas)));return destino;
}
int* grant(void* inventario,int* codigo,const void* init){
    ++otorgados;assert(inventario==holder.data());auto p=reinterpret_cast<uintptr_t>(init);
    assert(tomar<uint8_t>(p+n::initAbiertas)==5);
    if(variante!=5){
        poner(slots,n::itemUid,uint64_t(1000));poner(slots,n::itemTipo,tipoPrueba);poner(slots,n::itemCantidad,int64_t(1));
        poner(slots,n::itemAbiertas,uint8_t(variante==6?2:5));
    }
    *codigo=variante==4?1:0;return codigo;
}
int* originalTask(int* codigo,void* trabajo,void* ctx){
    ++tareasOriginales;argumentoError=codigo;argumentoTrabajo=trabajo;argumentoContexto=ctx;*codigo=0;return codigo;
}
void preparar(){
    raiz.fill(0);administrador.fill(0);metadata.fill(0);cliente.fill(0);autoridad.fill(0);componentes.fill(0);
    controlCliente.fill(0);controlServidor.fill(0);holder.fill(0);cubeta.fill(0);slots.fill(0);localTls.fill(0);def.fill(0);
    rootGlobal=ptr(raiz);base=reinterpret_cast<uintptr_t>(&rootGlobal)-n::globalRaiz;baseSimulada=base;
    poner(raiz,n::raizAdministrador,ptr(administrador));poner(administrador,0,base+n::vtableAdministrador);
    poner(administrador,n::administradorSeleccionado,ptr(cliente));poner(cliente,0,base+n::vtableSeleccionado);
    poner(autoridad,0,base+n::vtableAutoridad);
    for(auto p:{ptr(cliente),ptr(autoridad)}){
        poner(p+n::actorVivo,uint8_t(1));poner(p+n::actorDescriptor,ptr(metadata));poner(p+n::actorIdentidad,identidadPrueba);
    }
    poner(metadata,n::descriptorTipo,uint8_t(1));
    poner(cliente,n::actorControlador,ptr(controlCliente));poner(controlCliente,n::controladorActor,ptr(cliente));
    poner(autoridad,n::actorControlador,ptr(controlServidor));poner(controlServidor,n::controladorActor,ptr(autoridad));
    poner(autoridad,n::actorComponentes,ptr(componentes));poner(componentes,n::componentesInventario,ptr(holder));
    poner(holder,n::inventarioDueno,ptr(autoridad));cubetas[0]=ptr(cubeta);
    poner(holder,n::inventarioCubetas,reinterpret_cast<uintptr_t>(cubetas.data()));poner(holder,n::inventarioCantidad,uint32_t(1));
    poner(cubeta,n::cubetaSlots,ptr(slots));poner(cubeta,n::cubetaCantidad,uint16_t(4));
    for(size_t i=0;i<4;++i)poner(slots,i*n::pasoItem+n::itemTipo,uint16_t(0xffff));
    tlsGlobal=ptr(localTls);direccionTlsSimulada=reinterpret_cast<uintptr_t>(&tlsGlobal);
    poner(localTls,n::tlsRealm,uint8_t(1));poner(localTls,n::tlsContexto,uintptr_t(1));
    clavePrueba=n::clavesEquipo[0];poner(def,n::defClave,clavePrueba);poner(def,n::defDestino,uint16_t(0));
    poner(def,n::defAbiertas,uint8_t(0));poner(def,n::defMaxFisicas,uint32_t(5));
    buscar=lookup;definicion=definition;construir=construct;destruir=destroy;convertir=convert;otorgar=grant;original=originalTask;
    instalado=true;habilitado=false;bloqueado=false;ejecutando=false;procesando.clear();identidadConfirmada=0;ultimoContexto=0;pendiente.reset();
    contexto=ptr(autoridad);error=0;tiempoSimulado=10000;direccionRetornoSimulada=reinterpret_cast<void*>(base+n::retornoTareaA);
    construidos=destruidos=convertidos=otorgados=tareasOriginales=variante=0;denegar=0;
}
void tick(){assert(tarea(&error,reinterpret_cast<void*>(123),&contexto)==&error);}
void solicitar(){tick();activarEquipoNativo(true);assert(solicitarEquipoNativo(clavePrueba));}
}
namespace ds {
bool leer(uintptr_t p,void* destino,size_t n){if(p<0x10000||p==denegar)return false;std::memcpy(destino,reinterpret_cast<void*>(p),n);return true;}
bool escribirDatos(uintptr_t p,const void* origen,size_t n){if(p<0x10000||p==denegar)return false;std::memcpy(reinterpret_cast<void*>(p),origen,n);return true;}
}
int main(){
    assert(!ds::iniciarEquipoNativo());
    assert(!ds::equipoNativoDisponible());
    preparar();uint32_t identidad=0;assert(identidadSeleccionada()==identidadPrueba);
    assert(obtenerContexto(&contexto,&error,base+n::retornoTareaA,identidad)==ptr(holder));assert(identidad==identidadPrueba);
    assert(!obtenerContexto(&contexto,&error,base+1,identidad));
    poner(localTls,n::tlsRealm,uint8_t(0));assert(!obtenerContexto(&contexto,&error,base+n::retornoTareaA,identidad));
    preparar();tick();assert(tareasOriginales==1&&argumentoError==&error&&argumentoContexto==&contexto&&argumentoTrabajo==reinterpret_cast<void*>(123));
    assert(!solicitarEquipoNativo(clavePrueba));activarEquipoNativo(true);assert(!solicitarEquipoNativo(0));
    assert(solicitarEquipoNativo(clavePrueba));assert(!solicitarEquipoNativo(clavePrueba));tick();
    assert(construidos==1&&destruidos==1&&convertidos==1&&otorgados==1&&!bloqueado);
    assert(tomar<uint8_t>(ptr(slots)+n::itemAbiertas)==5);tick();assert(otorgados==1);
    preparar();solicitar();activarEquipoNativo(false);tick();assert(otorgados==0&&construidos==0);
    preparar();solicitar();tiempoSimulado+=30001;tick();assert(otorgados==0);
    preparar();solicitar();tiempoSimulado+=5001;tick();assert(otorgados==0);
    preparar();tick();activarEquipoNativo(true);ejecutando=true;assert(!solicitarEquipoNativo(clavePrueba));
    preparar();tick();activarEquipoNativo(true);tiempoSimulado+=5001;assert(!solicitarEquipoNativo(clavePrueba));
    for(unsigned caso:{1u,2u,3u,8u}){preparar();solicitar();variante=caso;tick();assert(otorgados==0&&!bloqueado);if(caso!=8)assert(destruidos==1);}
    for(unsigned caso:{4u,5u,6u,9u}){preparar();solicitar();variante=caso;tick();assert(bloqueado&&!equipoNativoActivo());assert(!solicitarEquipoNativo(clavePrueba));}
    preparar();solicitar();poner(cliente,n::actorIdentidad,identidadPrueba+1);tick();assert(otorgados==0);
    preparar();const auto catalogo=catalogoEquipoNativo();assert(catalogo.size()==std::size(n::clavesEquipo));
    assert(catalogo[0].claveExterna==n::clavesEquipo[0]);
    std::cout<<"Equipo nativo: contexto prestado, ABI, cinco registros, conversión, alta, UID, cancelación y bloqueo por resultado incierto aprobados.\n";
}
MH_STATUS MH_Initialize(){return MH_OK;}
MH_STATUS MH_CreateHook(void*,void*,void**){return MH_OK;}
MH_STATUS MH_EnableHook(void*){return MH_OK;}
MH_STATUS MH_DisableHook(void*){return MH_OK;}
MH_STATUS MH_RemoveHook(void*){return MH_OK;}
