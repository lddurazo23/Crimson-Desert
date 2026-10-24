#include "equipo_nativo.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <MinHook.h>
#include <bcrypt.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Adaptación de los contratos MIT de crimson-workbench, revisión 0c86f9b.
// live-items/{BUILD_2976.md,src/native.h,src/runtime.cpp,src/dispatch.h} y
// extra-sockets/{BUILD_2976.md,src/socket_policy.h}. Nunca mezcla ItemVal C0/C8.
namespace ds {
namespace {
namespace d = datos::equipoNativo;
using Buscar = bool(__fastcall*)(uint16_t*, uint32_t);
using Definicion = unsigned char*(__fastcall*)(const uint16_t*);
using Construir = void*(__fastcall*)(void*, const uint16_t*, int64_t);
using Convertir = void*(__fastcall*)(void*, const void*, uint16_t);
using Destruir = void(__fastcall*)(void*);
using Otorgar = int*(__fastcall*)(void*, int*, const void*);
using Tarea = int*(__fastcall*)(int*, void*, void*);
Buscar buscar{}; Definicion definicion{}; Construir construir{};
Convertir convertir{}; Destruir destruir{}; Otorgar otorgar{}; Tarea original{};
uintptr_t base = 0;
std::atomic_bool instalado{false}, habilitado{false}, bloqueado{false}, ejecutando{false};
std::atomic<uint32_t> identidadConfirmada{0};
std::atomic<ULONGLONG> ultimoContexto{0};
std::atomic_flag procesando = ATOMIC_FLAG_INIT;
std::mutex mutexSolicitud, mutexEstado;
struct Solicitud { uint32_t clave, identidad; ULONGLONG vence; };
std::optional<Solicitud> pendiente;
std::string mensaje = "no disponible: falta el perfil nativo publicado 2976";

void informar(const char* texto) {
    std::lock_guard<std::mutex> bloqueo(mutexEstado); mensaje = texto;
}
bool sumar(uintptr_t p, size_t n, uintptr_t& salida) {
    if(p < 0x10000 || n > UINTPTR_MAX-p) return false;
    salida = p+n; return true;
}
template<class T> bool campo(uintptr_t p, size_t n, T& valor) {
    uintptr_t direccion = 0; return sumar(p,n,direccion) && leer(direccion,valor);
}
uintptr_t puntero(uintptr_t p,size_t n) {
    uintptr_t resultado = 0; return campo(p,n,resultado) ? resultado : 0;
}
bool claveAdmitida(uint32_t clave) {
    return std::binary_search(std::begin(d::clavesEquipo),std::end(d::clavesEquipo),clave);
}
class Resumen {
    BCRYPT_ALG_HANDLE algoritmo{}; BCRYPT_HASH_HANDLE hash{};
public:
    Resumen() {
        if(BCryptOpenAlgorithmProvider(&algoritmo,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return;
        if(BCryptCreateHash(algoritmo,&hash,nullptr,0,nullptr,0,0)<0) hash=nullptr;
    }
    ~Resumen() { if(hash) BCryptDestroyHash(hash); if(algoritmo) BCryptCloseAlgorithmProvider(algoritmo,0); }
    bool agregar(const void* bytes,size_t n) {
        return hash && n<=ULONG_MAX && BCryptHashData(hash,
            reinterpret_cast<PUCHAR>(const_cast<void*>(bytes)),static_cast<ULONG>(n),0)>=0;
    }
    bool coincide(const char* esperado) {
        uint8_t bytes[32]{}; char texto[65]{};
        if(!hash || BCryptFinishHash(hash,bytes,sizeof(bytes),0)<0) return false;
        for(size_t i=0;i<sizeof(bytes);++i) std::snprintf(texto+2*i,3,"%02x",bytes[i]);
        return esperado && std::strcmp(texto,esperado)==0;
    }
};
bool archivoConfirmado(const wchar_t* ruta) {
    const auto archivo=CreateFileW(ruta,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,
        OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(archivo==INVALID_HANDLE_VALUE) return false;
    Resumen hash; std::array<uint8_t,65536> bytes{}; bool ok=true;
    for(;;) {
        DWORD n=0;
        if(!ReadFile(archivo,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr)) {ok=false;break;}
        if(!n) break;
        if(!hash.agregar(bytes.data(),n)) {ok=false;break;}
    }
    CloseHandle(archivo); return ok && hash.coincide(d::hashEjecutable);
}
bool codigoConfirmado(uintptr_t direccion,size_t n,const char* esperado) {
    Resumen hash; std::array<uint8_t,4096> bytes{};
    if(!n || n>1024*1024) return false;
    for(size_t cursor=0;cursor<n;) {
        const auto cantidad=std::min(bytes.size(),n-cursor);
        if(!leer(direccion+cursor,bytes.data(),cantidad) || !hash.agregar(bytes.data(),cantidad)) return false;
        cursor+=cantidad;
    }
    return hash.coincide(esperado);
}
bool validarPerfil() {
    wchar_t ruta[32768]{};
    if(!GetModuleFileNameW(nullptr,ruta,32768)) return false;
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    return leer(base,dos) && dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>0 &&
        dos.e_lfanew<1024*1024 && leer(base+dos.e_lfanew,nt) && nt.Signature==IMAGE_NT_SIGNATURE &&
        nt.FileHeader.TimeDateStamp==d::marcaTiempo && nt.OptionalHeader.SizeOfImage==d::tamanoImagen &&
        archivoConfirmado(ruta);
}
uint32_t identidadSeleccionada() {
    const auto raiz=puntero(base,d::globalRaiz);
    const auto administrador=puntero(raiz,d::raizAdministrador);
    if(puntero(administrador,0)!=base+d::vtableAdministrador) return 0;
    const auto seleccionado=puntero(administrador,d::administradorSeleccionado);
    if(puntero(seleccionado,0)!=base+d::vtableSeleccionado) return 0;
    uint8_t vivo=0,tipo=0;
    if(!campo(seleccionado,d::actorVivo,vivo) || !vivo ||
       !campo(puntero(seleccionado,d::actorDescriptor),d::descriptorTipo,tipo) || tipo!=1) return 0;
    const auto controlador=puntero(seleccionado,d::actorControlador);
    if(puntero(controlador,d::controladorActor)!=seleccionado) return 0;
    uint32_t identidad=0;
    return campo(seleccionado,d::actorIdentidad,identidad) && identidad &&
        puntero(administrador,d::administradorSeleccionado)==seleccionado ? identidad : 0;
}
uintptr_t tls() { return static_cast<uintptr_t>(__readgsqword(d::tebTls)); }
uintptr_t obtenerContexto(void* contexto,int* error,uintptr_t retorno,uint32_t& identidad) {
    if(retorno!=base+d::retornoTareaA && retorno!=base+d::retornoTareaB) return 0;
    int resultado=-1;
    if(!leer(reinterpret_cast<uintptr_t>(error),resultado) || resultado) return 0;
    const auto dueno=puntero(reinterpret_cast<uintptr_t>(contexto),0);
    if(puntero(dueno,0)!=base+d::vtableAutoridad) return 0;
    uint8_t realm=0xFF,tipo=0,vivo=0;
    const auto local=puntero(tls(),0);
    if(!campo(local,d::tlsRealm,realm) || realm!=1 || !puntero(local,d::tlsContexto)) return 0;
    const auto componentes=puntero(dueno,d::actorComponentes);
    const auto inventario=puntero(componentes,d::componentesInventario);
    if(!inventario || puntero(inventario,d::inventarioDueno)!=dueno ||
       !campo(puntero(dueno,d::actorDescriptor),d::descriptorTipo,tipo) || tipo!=1 ||
       !campo(dueno,d::actorVivo,vivo) || !vivo ||
       puntero(puntero(dueno,d::actorControlador),d::controladorActor)!=dueno ||
       !campo(dueno,d::actorIdentidad,identidad) || !identidad ||
       identidadSeleccionada()!=identidad) return 0;
    return inventario;
}
struct Pieza { uintptr_t slot; uint64_t uid; int64_t cantidad; uint8_t abiertas; };
bool piezas(uintptr_t inventario,uint16_t tipo,std::vector<Pieza>& salida,int64_t& total) {
    salida.clear(); total=0;
    const auto arreglo=puntero(inventario,d::inventarioCubetas); uint32_t n=0;
    if(!arreglo || !campo(inventario,d::inventarioCantidad,n) || !n || n>d::maxCubetas) return false;
    for(uint32_t b=0;b<n;++b) {
        const auto cubeta=puntero(arreglo,b*sizeof(uintptr_t));
        const auto slots=puntero(cubeta,d::cubetaSlots); uint16_t cantidad=0;
        if(!cubeta || !campo(cubeta,d::cubetaCantidad,cantidad) || cantidad>d::maxSlots || (!slots&&cantidad)) return false;
        for(uint16_t i=0;i<cantidad;++i) {
            const auto slot=slots+static_cast<size_t>(i)*d::pasoItem; uint16_t actual=d::tipoVacio;
            if(!campo(slot,d::itemTipo,actual)) return false;
            if(actual!=tipo) continue;
            Pieza pieza{slot,0,0,0};
            if(!campo(slot,d::itemUid,pieza.uid) || !pieza.uid || pieza.uid==UINT64_MAX ||
               !campo(slot,d::itemCantidad,pieza.cantidad) || pieza.cantidad<1 || pieza.cantidad>d::maxCantidad ||
               !campo(slot,d::itemAbiertas,pieza.abiertas) || pieza.abiertas>5 || total>d::maxCantidad-pieza.cantidad) return false;
            total+=pieza.cantidad; salida.push_back(pieza);
        }
    }
    return true;
}
// Estas envolturas SEH contienen únicamente locales triviales.
bool resolverTipo(uint32_t clave,uint16_t& tipo,uintptr_t& def) {
    __try { tipo=d::tipoVacio; if(!buscar(&tipo,clave) || tipo==d::tipoVacio) return false;
        def=reinterpret_cast<uintptr_t>(definicion(&tipo)); return def!=0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
enum class Resultado { Rechazado, Otorgado, Incierto };
Resultado crear(uintptr_t inventario,uint16_t tipo,uint32_t clave) {
    alignas(16) uint8_t item[d::pasoItem]{};
    alignas(16) uint8_t init[d::tamanoInit]{};
    bool construido=false;
    __try {
        construir(item,&tipo,1); construido=true;
        const auto raiz=reinterpret_cast<uintptr_t>(item);
        const auto datos=puntero(raiz,d::itemRanuras); uint32_t cantidad=0,capacidad=0; uint8_t abiertas=0;
        if(!datos || !campo(raiz,d::itemCantidadRanuras,cantidad) || cantidad!=5 ||
           !campo(raiz,d::itemCapacidadRanuras,capacidad) || capacidad<5 || capacidad>1024 ||
           !campo(raiz,d::itemAbiertas,abiertas) || abiertas>5) { destruir(item); return Resultado::Rechazado; }
        for(uint32_t i=0;i<5;++i) {
            uint16_t gear=d::tipoVacio; uint8_t indice=0;
            const auto registro=datos+i*d::pasoRanura;
            if(!campo(registro,d::ranuraGear,gear) || gear!=d::tipoVacio ||
               !campo(registro,d::ranuraIndice,indice) ||
               (i<abiertas ? indice!=i : indice!=0xFF)) { destruir(item); return Resultado::Rechazado; }
        }
        for(uint32_t i=0;i<5;++i) if(!escribir(datos+i*d::pasoRanura+d::ranuraIndice,static_cast<uint8_t>(i))) {
            destruir(item); return Resultado::Rechazado;
        }
        if(!escribir(raiz+d::itemAbiertas,uint8_t{5})) { destruir(item); return Resultado::Rechazado; }
        convertir(init,item,d::tipoVacio);
        uint64_t uid=0; uint32_t claveCreada=0; int64_t cuantos=0; uint8_t abiertasInit=0;
        const auto serializado=reinterpret_cast<uintptr_t>(init);
        if(!campo(serializado,d::initUid,uid) || uid!=UINT64_MAX ||
           !campo(serializado,d::initClave,claveCreada) || claveCreada!=clave ||
           !campo(serializado,d::initCantidad,cuantos) || cuantos!=1 ||
           !campo(serializado,d::initAbiertas,abiertasInit) || abiertasInit!=5) {
            destruir(item); return Resultado::Rechazado;
        }
        for(uint32_t i=0;i<5;++i) {
            uint8_t indice=0; uint16_t gear=0;
            if(!campo(serializado,d::initRanuras+i*d::pasoRanura+d::ranuraIndice,indice) || indice!=i ||
               !campo(serializado,d::initRanuras+i*d::pasoRanura+d::ranuraGear,gear) || gear!=d::tipoVacio) {
                destruir(item); return Resultado::Rechazado;
            }
        }
        destruir(item); construido=false;
        int error=-1; otorgar(reinterpret_cast<void*>(inventario),&error,init);
        return error==0 ? Resultado::Otorgado : Resultado::Incierto;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // Un constructor que falló no permite inventar cómo destruir su estado parcial.
        (void)construido; return Resultado::Incierto;
    }
}
void ejecutar(const Solicitud& solicitud,uintptr_t inventario) {
    uint16_t tipo=d::tipoVacio; uintptr_t def=0; uint32_t clave=0,maxFisicas=0;
    uint16_t destino=d::tipoVacio; uint8_t iniciales=0;
    if(!claveAdmitida(solicitud.clave) || !resolverTipo(solicitud.clave,tipo,def) ||
       !campo(def,d::defClave,clave) || clave!=solicitud.clave ||
       !campo(def,d::defDestino,destino) || (destino!=0 && destino!=1 && destino!=4 && destino!=13) ||
       !campo(def,d::defAbiertas,iniciales) || iniciales>5 ||
       !campo(def,d::defMaxFisicas,maxFisicas) || maxFisicas>5) {
        informar("Solicitud rechazada: la definición no admite esta ruta nativa de cinco ranuras."); return;
    }
    std::vector<Pieza> antes,despues; int64_t cantidadAntes=0,cantidadDespues=0;
    if(!piezas(inventario,tipo,antes,cantidadAntes)) {
        informar("Solicitud rechazada: no se pudo validar el inventario de autoridad."); return;
    }
    const auto resultado=crear(inventario,tipo,solicitud.clave);
    if(resultado==Resultado::Rechazado) {
        informar("La pieza temporal no pasó las validaciones de construcción o conversión; no se llamó a la transacción de alta."); return;
    }
    if(resultado==Resultado::Otorgado && piezas(inventario,tipo,despues,cantidadDespues) && cantidadDespues==cantidadAntes+1) {
        size_t nuevas=0; bool correcta=true;
        for(const auto& pieza:despues) {
            const bool existia=std::any_of(antes.begin(),antes.end(),[&](const Pieza& p){return p.uid==pieza.uid;});
            if(!existia) { ++nuevas; correcta=correcta && pieza.cantidad==1 && pieza.abiertas==5; }
        }
        if(nuevas==1 && correcta) {
            informar("El motor otorgó una pieza nueva con cinco ranuras y UID nuevo, verificada en autoridad. Comprueba el inventario visual y guarda/recarga una copia de tu partida."); return;
        }
    }
    bloqueado.store(true); habilitado.store(false);
    informar("Resultado incierto después de entrar a la ruta nativa: no se repetirá. Puede haberse creado una pieza; editor bloqueado. Revisa la partida y restaura el respaldo si hace falta.");
}
void despuesDeTarea(void* contexto,int* error,uintptr_t retorno) {
    if(bloqueado.load() || procesando.test_and_set(std::memory_order_acquire)) return;
    struct Liberar { ~Liberar(){procesando.clear(std::memory_order_release);} } liberar;
    uint32_t identidad=0;
    const auto inventario=obtenerContexto(contexto,error,retorno,identidad);
    if(!inventario) return;
    const auto ahora=GetTickCount64();
    const auto identidadAnterior=identidadConfirmada.exchange(identidad);
    const auto ultimoAnterior=ultimoContexto.exchange(ahora);
    if(!habilitado.load()) return;
    std::optional<Solicitud> solicitud;
    {
        std::unique_lock<std::mutex> lock(mutexSolicitud,std::try_to_lock);
        if(!lock.owns_lock() || !pendiente) return;
        solicitud=pendiente; pendiente.reset(); ejecutando.store(true);
    }
    struct Finalizar { ~Finalizar(){ejecutando.store(false);} } finalizar;
    if(!ultimoAnterior || ahora<ultimoAnterior || ahora-ultimoAnterior>5000 || identidadAnterior!=identidad) {
        informar("Solicitud cancelada: cambió la generación del personaje o se perdió el contexto nativo reciente."); return;
    }
    if(solicitud->vence<ahora || solicitud->identidad!=identidad) {
        informar("Solicitud cancelada: venció o cambió el personaje seleccionado."); return;
    }
    if(!habilitado.load() || identidadSeleccionada()!=identidad) {
        informar("Solicitud cancelada antes de construir la pieza."); return;
    }
    try { ejecutar(*solicitud,inventario); }
    catch(...) {
        bloqueado.store(true); habilitado.store(false);
        informar("Excepción durante la operación nativa; editor bloqueado para impedir una repetición incierta.");
    }
}
int* __fastcall tarea(int* error,void* trabajo,void* contexto) {
    const auto retorno=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto resultado=original(error,trabajo,contexto);
    despuesDeTarea(contexto,error,retorno);
    return resultado;
}
}
bool iniciarEquipoNativo() {
    if(instalado.load()) return true;
    base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!validarPerfil()) return false;
    for(const auto& pin:d::pins) if(!codigoConfirmado(base+pin.rva,pin.bytes,pin.hash)) {
        informar("no disponible: una rutina nativa no coincide con el perfil publicado 2976"); return false;
    }
    buscar=reinterpret_cast<Buscar>(base+d::buscarClave);
    definicion=reinterpret_cast<Definicion>(base+d::resolverDefinicion);
    construir=reinterpret_cast<Construir>(base+d::construirItem);
    convertir=reinterpret_cast<Convertir>(base+d::convertirInit);
    destruir=reinterpret_cast<Destruir>(base+d::destruirItem);
    otorgar=reinterpret_cast<Otorgar>(base+d::otorgarItem);
    const auto mh=MH_Initialize();
    if(mh!=MH_OK && mh!=MH_ERROR_ALREADY_INITIALIZED) return false;
    auto objetivo=reinterpret_cast<void*>(base+d::despacharTarea);
    if(MH_CreateHook(objetivo,reinterpret_cast<void*>(&tarea),reinterpret_cast<void**>(&original))!=MH_OK) return false;
    if(MH_EnableHook(objetivo)!=MH_OK) {MH_RemoveHook(objetivo);original=nullptr;return false;}
    instalado.store(true);
    informar("Alternativa nativa disponible y apagada: crea una pieza nueva con cinco ranuras. No modifica el equipo existente.");
    return true;
}
bool equipoNativoDisponible(){return instalado.load() && !bloqueado.load();}
bool equipoNativoActivo(){return habilitado.load() && equipoNativoDisponible();}
void activarEquipoNativo(bool activo) {
    habilitado.store(activo && equipoNativoDisponible());
    if(!activo) {std::lock_guard<std::mutex> lock(mutexSolicitud);pendiente.reset();}
}
bool solicitarEquipoNativo(uint32_t claveExterna) {
    if(!equipoNativoActivo() || ejecutando.load() || !claveAdmitida(claveExterna)) return false;
    const auto identidad=identidadSeleccionada(); const auto ahora=GetTickCount64();
    const auto ultimo=ultimoContexto.load();
    if(!identidad || identidadConfirmada.load()!=identidad || !ultimo || ahora<ultimo || ahora-ultimo>5000) {
        informar("Esperando una tarea nativa reciente del personaje de autoridad; no se encoló la pieza."); return false;
    }
    std::lock_guard<std::mutex> lock(mutexSolicitud);
    if(pendiente || ejecutando.load()) return false;
    pendiente=Solicitud{claveExterna,identidad,ahora+30000};
    informar("Creación de una pieza con cinco ranuras en cola; vence en 30 segundos y no se repite automáticamente.");
    return true;
}
std::vector<TipoEquipoNativo> catalogoEquipoNativo() {
    std::vector<TipoEquipoNativo> salida; salida.reserve(std::size(d::clavesEquipo));
    for(auto clave:d::clavesEquipo) salida.push_back({clave,"Equipo con clave real "+std::to_string(clave)});
    return salida;
}
const char* estadoEquipoNativo() {
    thread_local std::string copia;
    std::lock_guard<std::mutex> lock(mutexEstado);copia=mensaje;return copia.c_str();
}
}
