#include "monturas_nivel5.hpp"
#include "monturas5_contexto.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <cstddef>
#include <cstdint>

// Símbolos de nuestro gateway, no nombres internos del juego.
extern "C" {
void DsMonturasGateway();
uint64_t DsMonturasLeerXcr0();
void* DsMonturasTrampolin=nullptr;
uint32_t DsMonturasXsaveBytes=0,DsMonturasXsaveLo=0,DsMonturasXsaveHi=0;
extern const uint32_t DsMonturasMxcsr=0x1F80;
}

namespace ds {
namespace {
namespace d=datos::monturas5;
std::atomic_bool instalado{false},activo{false},fallo{false};
std::atomic<ULONGLONG> ultimoAjuste{0};
std::atomic<const char*> motivo{"no disponible: el perfil de velocidad de monturas no se ha validado"};

bool contextoExtendido() {
    int cpu[4]{};
    __cpuidex(cpu,0,0);
    if(cpu[0]<0x0D) return false;
    __cpuidex(cpu,1,0);
    // CPUID documentado por Intel: XSAVE y soporte del sistema operativo.
    if((static_cast<uint32_t>(cpu[2]) & ((1u<<26)|(1u<<27)))!=((1u<<26)|(1u<<27))) return false;
    const uint64_t mascara=DsMonturasLeerXcr0();
    if((mascara&3)!=3) return false;
    __cpuidex(cpu,0x0D,0);
    const uint32_t bytes=static_cast<uint32_t>(cpu[1]);
    const uint64_t soportado=static_cast<uint32_t>(cpu[0])|(uint64_t{static_cast<uint32_t>(cpu[3])}<<32);
    // Es un límite de recursos propio, no un tamaño supuesto del juego.
    if((mascara&~soportado)!=0 || bytes<576 || bytes>32768) return false;
    DsMonturasXsaveBytes=bytes;
    DsMonturasXsaveLo=static_cast<uint32_t>(mascara);
    DsMonturasXsaveHi=static_cast<uint32_t>(mascara>>32);
    return true;
}
bool leerCampo(uintptr_t campo,monturas5::CampoNivel& v) {
    return leer(campo+d::actual,v.actual) && leer(campo+d::maximo,v.maximo) &&
        leer(campo+d::minimo,v.minimo) && leer(campo+d::paso,v.paso) &&
        leer(campo+d::estadistica,v.estadistica) && leer(campo+d::subtipo,v.subtipo) &&
        leer(campo+d::ordinal,v.ordinal);
}
bool perfilValido(const monturas5::CampoNivel& v) {
    return v.estadistica==d::tipoEstadistica && v.subtipo==d::subtipoVelocidad &&
        v.ordinal==d::ordinalVelocidad && v.maximo==d::maximoVelocidad &&
        v.minimo==d::minimoVelocidad && v.paso==d::pasoVelocidad && v.actual<=v.maximo;
}
bool mismoCampo(const monturas5::CampoNivel& a,const monturas5::CampoNivel& b) {
    return a.actual==b.actual && a.maximo==b.maximo && a.minimo==b.minimo && a.paso==b.paso &&
        a.estadistica==b.estadistica && a.subtipo==b.subtipo && a.ordinal==b.ordinal;
}
// No contiene objetos con destructor: el error nativo se contiene en esta llamada.
bool compararIntercambiar(uintptr_t p,uint64_t previo,uint64_t nuevo,bool& cambiado) {
    cambiado=false;
    MEMORY_BASIC_INFORMATION pagina{};
    if((p&7)!=0 || !VirtualQuery(reinterpret_cast<void*>(p),&pagina,sizeof(pagina)) ||
       pagina.State!=MEM_COMMIT || (pagina.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
    const DWORD permiso=pagina.Protect&0xFF;
    if(permiso!=PAGE_READWRITE && permiso!=PAGE_WRITECOPY) return false;
    const auto base=reinterpret_cast<uintptr_t>(pagina.BaseAddress);
    if(p<base || pagina.RegionSize<sizeof(uint64_t) || p-base>pagina.RegionSize-sizeof(uint64_t)) return false;
    __try {
        const auto encontrado=static_cast<uint64_t>(_InterlockedCompareExchange64(
            reinterpret_cast<volatile long long*>(p),static_cast<long long>(nuevo),static_cast<long long>(previo)));
        cambiado=encontrado==previo;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

// El gateway salva TODO el contexto antes de llamar. Esta función solo conoce
// el marco propio y los registros documentados en la investigación citada.
extern "C" void DsMonturasCallback(ContextoMonturas5* c) noexcept {
    if(!c || !activo.load(std::memory_order_relaxed) || fallo.load()) return;
    try {
        const uintptr_t campo=c->r12,base=c->r14;
        if(campo<0x10000 || base<0x10000 || campo<base || campo-base!=d::distanciaCampo || (campo&7)!=0) return;
        monturas5::CampoNivel a{},b{};
        if(!leerCampo(campo,a) || !perfilValido(a) || !leerCampo(campo,b) || !mismoCampo(a,b)) return;
        bool cambiado=false;
        if(!compararIntercambiar(campo+d::actual,a.actual,d::maximoVelocidad,cambiado)) {
            fallo.store(true); activo.store(false);
            motivo.store("Velocidad de monturas desactivada: el campo rechazó una escritura atómica.");
            return;
        }
        if(!cambiado) return;
        // La ruta publicada necesita ambas acciones: dato y argumento RDX.
        c->rdx=d::maximoVelocidad;
        ultimoAjuste.store(GetTickCount64(),std::memory_order_relaxed);
    } catch(...) {
        fallo.store(true); activo.store(false);
        motivo.store("Velocidad de monturas desactivada por una excepción; reinicia el juego.");
    }
}

bool iniciarMonturasNivel5() {
    if(instalado.load()) return true;
    if(!contextoExtendido()) {
        motivo.store("no disponible: no se puede preservar el contexto extendido XSAVE de este equipo"); return false;
    }
    const auto guardia=buscarUnico(d::firmaGuardia),paso=buscarUnico(d::firmaPaso);
    if(!guardia || !paso || paso<guardia || paso-guardia!=d::distanciaGuardia || !ejecutable(paso)) {
        motivo.store("no disponible: faltan datos; las dos firmas únicas de velocidad no coinciden con el perfil publicado"); return false;
    }
    const auto mh=MH_Initialize();
    if(mh!=MH_OK && mh!=MH_ERROR_ALREADY_INITIALIZED) return false;
    void* objetivo=reinterpret_cast<void*>(paso);
    const auto creado=MH_CreateHook(objetivo,reinterpret_cast<void*>(&DsMonturasGateway),&DsMonturasTrampolin);
    if(creado!=MH_OK || !DsMonturasTrampolin) {
        if(creado==MH_OK) MH_RemoveHook(objetivo);
        motivo.store("no disponible: no se pudo construir el gateway de velocidad"); return false;
    }
    if(MH_EnableHook(objetivo)!=MH_OK) {
        MH_RemoveHook(objetivo); DsMonturasTrampolin=nullptr;
        motivo.store("no disponible: no se pudo activar el gateway de velocidad"); return false;
    }
    instalado.store(true);
    motivo.store("Disponible: nivel 5 global para campos compatibles; opción desactivada.");
    return true;
}
bool monturasNivel5Disponible() {return instalado.load() && !fallo.load();}
bool monturasNivel5Activas() {return activo.load() && monturasNivel5Disponible();}
void activarMonturasNivel5(bool activar) {activo.store(activar && monturasNivel5Disponible());}
const char* estadoMonturasNivel5() {
    if(!monturasNivel5Disponible()) return motivo.load();
    if(!activo.load()) return "Nivel 5 global desactivado; no se revierten cambios ya aceptados por el juego.";
    const auto ahora=GetTickCount64(),ultimo=ultimoAjuste.load();
    return ultimo && ahora>=ultimo && ahora-ultimo<5000 ?
        "Nivel 5 aplicado a un campo compatible; cobertura por especie y persistencia sin prueba en este proyecto." :
        "Nivel 5 global activado; esperando el acceso nativo a velocidad de una montura compatible.";
}
}
