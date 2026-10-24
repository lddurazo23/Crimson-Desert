#include "regiones.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include "vitales.hpp"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace ds {
uintptr_t resolverTablaPublica(const char* nombre, bool indirecta);
namespace {
namespace d = datos::regiones;
// ABI publicado, no deducido de una firma desconocida: skills.cpp, revisión fijada.
using Otorgar = uint32_t* (__fastcall*)(uintptr_t,uint32_t*,const uint16_t*,uint32_t,
                                      uint32_t,uint8_t,void*);
Otorgar otorgar = nullptr;
using ConsultarHilo = LONG(NTAPI*)(HANDLE,ULONG,PVOID,ULONG,PULONG);
ConsultarHilo consultarHilo = nullptr;
uintptr_t globalConocimiento = 0;
std::atomic_bool instalado{false}, solicitado{false}, bloqueado{false};
std::atomic<uint64_t> revision{0};
std::atomic_flag procesando = ATOMIC_FLAG_INIT;
std::mutex mutexSolicitud;
std::vector<Descubrimiento> copiaCatalogo;
std::string mensaje = "no disponible: faltan datos del editor de descubrimientos (firmas aún no validadas).";
struct Solicitud { uint32_t clave; uint64_t revision; };
std::deque<Solicitud> cola;
struct Contexto {
    uintptr_t propietario=0,componente=0,estados=0,tabla=0,filas=0;
    uint32_t cantidad=0;
};
struct Trabajo {
    bool activo=false;
    uint32_t clave=0;
    uint16_t fila=0;
    uint64_t revision=0;
    Contexto contexto{};
    ULONGLONG vence=0;
} trabajo;
ULONGLONG ultimaLectura=0;

uintptr_t campo(uintptr_t base,ptrdiff_t offset) {
    if(base<0x10000 || offset<0 || static_cast<uintptr_t>(offset)>UINTPTR_MAX-base) return 0;
    return base+static_cast<uintptr_t>(offset);
}
uintptr_t puntero(uintptr_t sitio) {
    uintptr_t resultado=0;
    return sitio && leer(sitio,resultado) && resultado>=0x10000 ? resultado : 0;
}
void informar(std::string texto) {
    std::lock_guard<std::mutex> lock(mutexSolicitud); mensaje=std::move(texto);
}
bool texto(uintptr_t p,std::string& salida) {
    salida.clear();
    if(!p) return false;
    for(size_t i=0;i<256;++i) {
        char c=0;
        if(p>UINTPTR_MAX-i || !leer(p+i,c)) return false;
        if(!c) return !salida.empty();
        if(static_cast<unsigned char>(c)<0x20) return false;
        salida.push_back(c);
    }
    return false;
}
bool claseCorrecta(uintptr_t objeto) {
    const auto vt=puntero(objeto);
    const auto col=vt>=8 ? puntero(vt-8) : 0;
    uint32_t rva=0;
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    std::string nombre;
    // Estructura RTTI x64 de MSVC; estos campos pertenecen al ABI público.
    return col && base && leer(col+0x0C,rva) && rva<1024u*1024u*1024u &&
        base<=UINTPTR_MAX-static_cast<uintptr_t>(rva)-0x10 &&
        texto(base+rva+0x10,nombre) && nombre==d::claseConocimiento;
}
bool contextoActual(Contexto& c) {
    c={};
    c.propietario=propietarioControlado();
    c.tabla=puntero(globalConocimiento);
    if(!c.propietario || !c.tabla || !leer(campo(c.tabla,d::tablaCantidad),c.cantidad) ||
       !c.cantidad || c.cantidad>=65535) return false;
    c.filas=puntero(campo(c.tabla,d::tablaFilas));
    const auto componentes=puntero(campo(c.propietario,d::propietarioComponentes));
    c.componente=puntero(campo(componentes,d::componenteConocimiento));
    uint32_t cantidad=0;
    if(!c.filas || !claseCorrecta(c.componente) ||
       !leer(campo(c.componente,d::conocimientoCantidad),cantidad) || cantidad!=c.cantidad) return false;
    c.estados=puntero(campo(c.componente,d::conocimientoEstados));
    uint8_t ultimo=0;
    constexpr auto maximoPaso=static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max())/65535u;
    if constexpr(d::pasoEstadoConocimiento==0 || d::pasoEstadoConocimiento>maximoPaso) return false;
    return c.estados && leer(campo(c.estados,static_cast<ptrdiff_t>((c.cantidad-1u)*d::pasoEstadoConocimiento)),ultimo);
}
bool mismoContexto(const Contexto& a,const Contexto& b) {
    return a.propietario==b.propietario && a.componente==b.componente &&
        a.estados==b.estados && a.tabla==b.tabla && a.filas==b.filas && a.cantidad==b.cantidad;
}
bool categoria(const std::string& nombre,ClaseDescubrimiento& clase) {
    std::string_view vista(nombre);
    const std::string_view prefijo(d::prefijoConocimiento);
    if(!vista.starts_with(prefijo)) return false;
    vista.remove_prefix(prefijo.size());
    const auto fin=vista.find('_');
    const auto token=vista.substr(0,fin);
    if(token==d::categoriaLugares) {clase=ClaseDescubrimiento::Lugar;return true;}
    if(token==d::categoriaAbismo) {clase=ClaseDescubrimiento::Abismo;return true;}
    return false;
}
bool leerFila(const Contexto& c,uint16_t indice,Descubrimiento& fila) {
    if(indice>=c.cantidad) return false;
    const auto def=puntero(campo(c.filas,static_cast<ptrdiff_t>(indice)*sizeof(uintptr_t)));
    if(!def || !leer(campo(def,d::filaClave),fila.clave)) return false;
    const auto nombre=puntero(puntero(campo(def,d::filaNombre)));
    if(!texto(nombre,fila.nombreInterno) || !categoria(fila.nombreInterno,fila.clase)) return false;
    uint8_t nivel=0;
    if(!leer(campo(c.estados,static_cast<ptrdiff_t>(indice)*d::pasoEstadoConocimiento),nivel)) return false;
    fila.aprendido=nivel>0; return true;
}
bool catalogo(const Contexto& c,std::vector<Descubrimiento>& salida,uint32_t claveBuscada=0,
              uint16_t* indiceBuscado=nullptr) {
    salida.clear();
    std::unordered_map<uint32_t,uint16_t> identificadores;
    for(uint32_t i=0;i<c.cantidad;++i) {
        Descubrimiento item;
        if(!leerFila(c,static_cast<uint16_t>(i),item)) continue;
        if(!identificadores.emplace(item.clave,static_cast<uint16_t>(i)).second) return false;
        if(indiceBuscado && item.clave==claveBuscada) *indiceBuscado=static_cast<uint16_t>(i);
        salida.push_back(std::move(item));
    }
    Contexto vigente;
    return contextoActual(vigente) && mismoContexto(c,vigente);
}
void detener(std::string motivo,bool fallo=false) {
    trabajo={};
    if(fallo) {bloqueado=true;solicitado=false;}
    informar(std::move(motivo));
}
// El aislamiento SEH no contiene objetos con destructor, compatible con MSVC.
bool llamadaNativa(uintptr_t componente,uint16_t fila,uint32_t* error) {
    __try {otorgar(componente,error,&fila,1,0,d::fuenteOtorgamiento,nullptr);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
uintptr_t realmActual(uint8_t& valor) {
    valor=0xFF;
    if(!consultarHilo) return 0;
    uint8_t informacion[d::tamanoInformacionHilo]{};
    if(consultarHilo(GetCurrentThread(),0,informacion,sizeof(informacion),nullptr)<0) return 0;
    uintptr_t teb=0;
    std::memcpy(&teb,informacion+d::informacionTeb,sizeof(teb));
    const auto arregloTls=puntero(campo(teb,d::tebPunterosTls));
    const auto tls=puntero(arregloTls);
    const auto flag=campo(tls,d::campoRealm);
    return flag && leer(flag,valor) && valor<=1 ? flag : 0;
}
bool aprender(const Contexto& c,uint16_t fila,uint32_t& error,bool& excepcion) {
    uint8_t anterior=0xFF;
    const auto realm=realmActual(anterior);
    if(!realm || anterior>1) return false;
    if(!escribir(realm,uint8_t{1})) return false;
    uint8_t lectura=0xFF;
    if(!leer(realm,lectura) || lectura!=1) {
        if(!escribir(realm,anterior)) bloqueado=true;
        return false;
    }
    bool llamada=false;
    try {llamada=llamadaNativa(c.componente,fila,&error);}
    catch(...) {
        const bool restaurado=escribir(realm,anterior);
        if(!restaurado) bloqueado=true;
        throw;
    }
    // La autoridad temporal siempre termina antes de volver al tick del juego.
    if(!escribir(realm,anterior) || !leer(realm,lectura) || lectura!=anterior) {
        bloqueado=true; return false;
    }
    excepcion=!llamada;
    return llamada && error==0;
}
void limpiarCola() {
    std::lock_guard<std::mutex> lock(mutexSolicitud); cola.clear(); copiaCatalogo.clear();
}
}

bool iniciarRegiones() {
    if(instalado.load()) return true;
    if(!iniciarResolucionPersonaje()) return false;
    const auto ntdll=GetModuleHandleW(L"ntdll.dll");
    consultarHilo=ntdll ? reinterpret_cast<ConsultarHilo>(GetProcAddress(ntdll,"NtQueryInformationThread")) : nullptr;
    if(!consultarHilo) return false;
    const auto sitio=buscarUnico(d::firmaOtorgar);
    const auto llamada=campo(sitio,static_cast<ptrdiff_t>(d::sitioLlamada));
    uint8_t opcode=0;
    if(!llamada || !leer(llamada,opcode) || opcode!=0xE8) return false;
    const auto funcion=resolverRip(llamada,1,5);
    if(!funcion || !ejecutable(funcion)) return false;
    uintptr_t global=0;
    if(d::firmaTablaConocimiento && *d::firmaTablaConocimiento) {
        const auto patron=buscarUnico(d::firmaTablaConocimiento);
        if(patron && d::longitudTablaConocimiento>=5 && d::longitudTablaConocimiento<=32 &&
           d::ripTablaConocimiento<=d::longitudTablaConocimiento-4)
            global=resolverRip(patron,d::ripTablaConocimiento,d::longitudTablaConocimiento);
    } else global=resolverTablaPublica("knowledgeinfo",false);
    if(!global) return false;
    globalConocimiento=global; otorgar=reinterpret_cast<Otorgar>(funcion); instalado=true;
    informar("Editor de descubrimientos disponible; habilítalo para leer lugares y entradas del abismo del catálogo real.");
    return true;
}
size_t regionCantidad() {return d::nombresRegiones.size();}
const char* nombreRegion(size_t region) {
    return region<d::nombresRegiones.size() ? d::nombresRegiones[region] : "Región inválida";
}
bool descubrimientosDisponible() {return instalado.load() && !bloqueado.load();}
void activarConocimientos(bool activo) {
    const bool valor=activo && descubrimientosDisponible();
    if(solicitado.exchange(valor)!=valor) revision.fetch_add(1);
    if(!valor) limpiarCola();
}
bool conocimientosActivos() {return solicitado.load() && descubrimientosDisponible();}
std::vector<Descubrimiento> leerDescubrimientos() {
    std::lock_guard<std::mutex> lock(mutexSolicitud); return copiaCatalogo;
}
bool solicitarAprenderClave(uint32_t clave) {
    if(!conocimientosActivos()) return false;
    std::lock_guard<std::mutex> lock(mutexSolicitud);
    const auto encontrado=std::find_if(copiaCatalogo.begin(),copiaCatalogo.end(),
        [clave](const auto& x){return x.clave==clave && !x.aprendido;});
    if(encontrado==copiaCatalogo.end() || cola.size()>=8 ||
       std::any_of(cola.begin(),cola.end(),[clave](const auto& x){return x.clave==clave;})) return false;
    cola.push_back({clave,revision.load()});
    mensaje="Conocimiento en cola; el motor confirmará el resultado. No equivale a activar una puerta.";
    return true;
}
const char* estadoRegiones() {
    thread_local std::string copia;
    std::lock_guard<std::mutex> lock(mutexSolicitud); copia=mensaje; return copia.c_str();
}
void procesarRegiones() {
    if(!conocimientosActivos() || procesando.test_and_set(std::memory_order_acquire)) return;
    struct Liberar {~Liberar(){procesando.clear(std::memory_order_release);}} liberar;
    try {
        const auto version=revision.load();
        if(trabajo.activo && trabajo.revision!=version) trabajo={};
        Contexto c;
        if(!contextoActual(c)) {
            if(trabajo.activo) detener("Acción detenida: cambió el actor o el contexto del conocimiento.");
            limpiarCola(); informar("Esperando el personaje y su catálogo real de conocimientos."); return;
        }
        const auto ahora=GetTickCount64();
        if(trabajo.activo) {
            if(!mismoContexto(c,trabajo.contexto)) {detener("Acción detenida: cambió el personaje, la tabla o la sesión.");return;}
            Descubrimiento actual;
            if(!leerFila(c,trabajo.fila,actual) || actual.clave!=trabajo.clave) {
                detener("Acción detenida: cambió la identidad del conocimiento.");return;
            }
            if(actual.aprendido) {
                trabajo={}; ultimaLectura=0;
                informar("Conocimiento confirmado por el motor. Comprueba el lugar o icono y guarda sólo después de verificarlo.");
            } else if(ahora>=trabajo.vence) {detener("El juego no confirmó el conocimiento a tiempo; no se repetirá la llamada.");return;}
            else return;
        }
        if(!ultimaLectura || ahora-ultimaLectura>=500) {
            std::vector<Descubrimiento> items;
            if(!catalogo(c,items)) {detener("Catálogo rechazado: claves duplicadas o cambio de contexto.");return;}
            std::unique_lock<std::mutex> lock(mutexSolicitud,std::try_to_lock);
            if(!lock.owns_lock()) return;
            copiaCatalogo=std::move(items); ultimaLectura=ahora;
        }
        Solicitud solicitud{};
        {
            std::unique_lock<std::mutex> lock(mutexSolicitud,std::try_to_lock);
            if(!lock.owns_lock() || cola.empty()) return;
            solicitud=cola.front();cola.pop_front();
        }
        if(solicitud.revision!=revision.load() || !conocimientosActivos()) return;
        uint16_t fila=UINT16_MAX;
        std::vector<Descubrimiento> items;
        if(!catalogo(c,items,solicitud.clave,&fila) || fila==UINT16_MAX) {
            detener("Clave rechazada: ya no existe en las categorías de descubrimiento permitidas.");return;
        }
        Descubrimiento actual;
        Contexto vigente;
        if(!leerFila(c,fila,actual) || actual.clave!=solicitud.clave ||
           !contextoActual(vigente) || !mismoContexto(c,vigente)) {
            detener("La identidad cambió antes de aplicar el conocimiento.");return;
        }
        if(actual.aprendido) {ultimaLectura=0;informar("Ese conocimiento ya estaba aprendido; no se hizo otra llamada.");return;}
        uint32_t error=UINT32_MAX;bool excepcion=false;
        if(!aprender(c,fila,error,excepcion)) {
            detener(excepcion ? "Excepción nativa: editor bloqueado para esta sesión." :
                "El motor rechazó el conocimiento o no se pudo restaurar su contexto de autoridad.",excepcion || bloqueado.load());return;
        }
        trabajo={true,solicitud.clave,fila,solicitud.revision,c,ahora+5000};
        informar("Solicitud nativa enviada; esperando que el juego confirme el conocimiento.");
    } catch(...) {detener("Excepción al procesar descubrimientos; editor bloqueado para esta sesión.",true);}
}
}
