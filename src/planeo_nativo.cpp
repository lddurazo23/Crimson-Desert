#include "planeo_nativo.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <bcrypt.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Adaptación de crimson-workbench (MIT), revisión fijada en datos_juego.hpp.
// Toda dirección, huella de animación y offset del juego vive en ese encabezado.
namespace ds {
namespace {
namespace d=datos::planeoNativo;
using Direccion=uintptr_t;
using VelocidadNativa=uintptr_t(__fastcall*)(void*,const void*,void*);
std::atomic<VelocidadNativa> velocidadOriginal{nullptr};
std::atomic_bool conectado{false}, habilitado{false}, fallo{false}, boton{false};
std::atomic<float> adelante{0}, vertical{0}, factorElegido{1}, rapidezElegida{0};
std::atomic<ULONGLONG> ultimoInput{0}, ultimaAplicacion{0};
Direccion baseJuego=0;
const char* motivo="no disponible: el perfil nativo de planeo todavía no se ha verificado";
struct Lectura {
    bool valida=true;
    template<class T> T at(Direccion p) {
        T valor{};
        if(!p || !leer(p,valor)) valida=false;
        return valor;
    }
};
// La captura no conserva objetos entre llamadas: todos los vínculos se leen de nuevo.
struct Contexto {
    Direccion raiz{},administrador{},actor{},metadatos{},componentes{},posesor{},control{},estado{},integrador{},metodo{};
    Direccion selector{},capa{},animacion{},fija{},paquete{},vista{},instancias{},condiciones{},buffer{};
    uint64_t huella{};
    uint32_t identidad{},cantidadComponentes{};
    uint8_t tipo{},modo{},indiceCapa{},eventos{};
    bool operator==(const Contexto&) const=default;
};
struct AnimacionesPublicadas {
    bool operator()(uint64_t huella,uint8_t eventos,uint8_t capa) const {
        if(capa==1) {
            for(const auto& a:d::kAnimations) if(a.fingerprint==huella && a.events==eventos) return true;
        } else if(capa==0) {
            for(const auto& a:d::kUpperAnimations) if(a.fingerprint==huella && a.events==eventos) return true;
        }
        return false;
    }
};
bool paqueteValido(uint8_t capa,uint32_t eventos,uint32_t condiciones,uint32_t buscadores) {
    return capa==0 ? eventos==d::eventosSuperior && condiciones==d::condicionesSuperior && buscadores==d::buscadoresSuperior :
        capa==1 && eventos==d::eventosInferior && condiciones==d::condicionesInferior && buscadores==d::buscadoresInferior;
}
template<class Clasificar=AnimacionesPublicadas>
bool capturar(Direccion base,Direccion metodo,Contexto& salida,Clasificar clasificar={}) {
    Lectura r;
    if(!base || !metodo || r.at<Direccion>(metodo)!=base+d::vtMetodo) return false;
    Contexto c{};c.metodo=metodo;
    c.control=r.at<Direccion>(metodo+d::metodoControl);c.actor=r.at<Direccion>(metodo+d::metodoActor);
    c.raiz=r.at<Direccion>(base+d::raizJugador);
    if(!c.raiz || !c.actor || !c.control) return false;
    c.administrador=r.at<Direccion>(c.raiz+d::raizAdministrador);
    if(!c.administrador || r.at<Direccion>(c.administrador)!=base+d::vtAdministrador ||
       r.at<Direccion>(c.administrador+d::administradorActor)!=c.actor ||
       r.at<Direccion>(c.actor)!=base+d::vtActor || r.at<uint8_t>(c.actor+d::actorVivo)!=1) return false;
    c.identidad=r.at<uint32_t>(c.actor+d::actorIdentidad);
    c.metadatos=r.at<Direccion>(c.actor+d::actorMetadatos);
    c.componentes=r.at<Direccion>(c.actor+d::actorComponentes);
    c.cantidadComponentes=r.at<uint32_t>(c.actor+d::actorCantidadComponentes);
    c.posesor=r.at<Direccion>(c.actor+d::actorPosesor);
    if(!c.identidad || !c.metadatos || !c.componentes || c.cantidadComponentes<9 || c.cantidadComponentes>57 ||
       !c.posesor || r.at<Direccion>(c.posesor+d::posesorActor)!=c.actor) return false;
    c.tipo=r.at<uint8_t>(c.metadatos+d::metadatosTipo);
    if((c.tipo!=1 && c.tipo!=4) || r.at<Direccion>(c.componentes+d::componentesControl)!=c.control ||
       r.at<Direccion>(c.control)!=base+d::vtControl || r.at<Direccion>(c.control+d::controlActor)!=c.actor) return false;
    c.estado=r.at<Direccion>(c.control+d::controlEstado);c.integrador=r.at<Direccion>(c.control+d::controlIntegrador);
    if(!c.estado || !c.integrador || r.at<Direccion>(c.estado+d::estadoControl)!=c.control ||
       r.at<Direccion>(c.estado+d::estadoActor)!=c.actor) return false;
    c.modo=r.at<uint8_t>(c.estado+d::estadoModo);
    if(c.modo>3 || r.at<Direccion>(c.estado+d::estadoMetodos+Direccion{c.modo}*sizeof(Direccion))!=metodo) return false;
    c.selector=r.at<Direccion>(c.control+d::controlSelector);
    if(!c.selector) return false;
    c.indiceCapa=r.at<uint8_t>(c.selector+d::selectorCapa);
    if(c.indiceCapa>1) return false;
    c.capa=r.at<Direccion>(c.control+d::controlCapas+Direccion{c.indiceCapa}*sizeof(Direccion));
    if(!c.capa) return false;
    c.animacion=r.at<Direccion>(c.capa+d::capaAnimacion);
    if(!c.animacion) return false;
    c.fija=r.at<Direccion>(c.animacion+d::animacionFija);
    c.paquete=r.at<Direccion>(c.animacion+d::animacionPaquete);
    c.vista=r.at<Direccion>(c.animacion+d::animacionVista);
    c.condiciones=r.at<Direccion>(c.animacion+d::animacionCondiciones);
    if(!c.fija || c.fija>UINTPTR_MAX-d::longitudAnimacion || !c.paquete || !c.vista || !c.condiciones ||
       !paqueteValido(c.indiceCapa,r.at<uint32_t>(c.paquete+d::paqueteEventos),
           r.at<uint32_t>(c.paquete+d::paqueteCondiciones),r.at<uint32_t>(c.paquete+d::paqueteBuscadores))) return false;
    c.eventos=r.at<uint8_t>(c.vista+d::vistaCantidadEventos);
    c.instancias=r.at<Direccion>(c.vista+d::vistaInstancias);c.buffer=r.at<Direccion>(c.paquete+d::paqueteBuffer);
    if(!c.instancias || !c.buffer) return false;
    // FNV-1a es el algoritmo publicado; se leen todos los bytes de la animación.
    c.huella=14695981039346656037ull;
    for(size_t i=0;i<d::longitudAnimacion;++i) c.huella=(c.huella^r.at<uint8_t>(c.fija+i))*1099511628211ull;
    if(!r.valida || !clasificar(c.huella,c.eventos,c.indiceCapa)) return false;
    // Una captura interrumpida por carga, cambio de actor o transición no autoriza editar.
    const bool mismo=r.at<Direccion>(metodo)==base+d::vtMetodo &&
        r.at<Direccion>(metodo+d::metodoControl)==c.control && r.at<Direccion>(metodo+d::metodoActor)==c.actor &&
        r.at<Direccion>(base+d::raizJugador)==c.raiz && r.at<Direccion>(c.raiz+d::raizAdministrador)==c.administrador &&
        r.at<Direccion>(c.administrador+d::administradorActor)==c.actor && r.at<uint32_t>(c.actor+d::actorIdentidad)==c.identidad &&
        r.at<uint8_t>(c.actor+d::actorVivo)==1 && r.at<Direccion>(c.control+d::controlEstado)==c.estado &&
        r.at<Direccion>(c.estado+d::estadoMetodos+Direccion{c.modo}*sizeof(Direccion))==metodo &&
        r.at<Direccion>(c.control+d::controlSelector)==c.selector && r.at<uint8_t>(c.selector+d::selectorCapa)==c.indiceCapa &&
        r.at<Direccion>(c.control+d::controlCapas+Direccion{c.indiceCapa}*sizeof(Direccion))==c.capa &&
        r.at<Direccion>(c.capa+d::capaAnimacion)==c.animacion && r.at<Direccion>(c.animacion+d::animacionFija)==c.fija;
    if(!mismo || !r.valida) return false;
    salida=c;return true;
}
bool llamadaPublicada(Direccion base,Direccion llamada) {
    if(llamada<base) return false;
    for(auto retorno:d::kReturns) if(llamada-base==retorno) return true;
    return false;
}
bool compararSeguro(Direccion p,uint32_t antes,uint32_t despues) {
    if(!p || (p&3)) return false;
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(reinterpret_cast<void*>(p),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
    const DWORD permiso=m.Protect&0xff;
    if(permiso!=PAGE_READWRITE && permiso!=PAGE_WRITECOPY) return false;
    const auto inicio=reinterpret_cast<Direccion>(m.BaseAddress);
    if(p<inicio || p-inicio>m.RegionSize || sizeof(uint32_t)>m.RegionSize-(p-inicio)) return false;
    __try {
        return static_cast<uint32_t>(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(p),
            static_cast<LONG>(despues),static_cast<LONG>(antes)))==antes;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
struct Velocidad {std::array<uint32_t,3> antes{},despues{};};
bool preparar(Direccion salida,float factor,float ordenVertical,float rapidez,bool inverso,Velocidad& v) {
    if(!salida || (salida&3) || salida>UINTPTR_MAX-d::salidaVelocidad-3*sizeof(float) ||
       !std::isfinite(factor) || factor<1 || factor>5 || !std::isfinite(ordenVertical) ||
       ordenVertical < -1 || ordenVertical>1 || !std::isfinite(rapidez) || rapidez<0 || rapidez>50) return false;
    for(size_t i=0;i<3;++i) {
        if(!leer(salida+d::salidaVelocidad+i*sizeof(float),v.antes[i])) return false;
        float f=0;std::memcpy(&f,&v.antes[i],sizeof(f));
        if(!std::isfinite(f) || std::fabs(f)>10000) return false;
        if(i!=1) f=inverso ? f/factor : f*factor;
        else if(!inverso && ordenVertical!=0) f=ordenVertical*rapidez;
        if(!std::isfinite(f) || std::fabs(f)>10000) return false;
        std::memcpy(&v.despues[i],&f,sizeof(f));
    }
    return true;
}
bool publicar(Direccion salida,const Velocidad& v) {
    size_t intentados=0;
    for(;intentados<3;++intentados) {
        const auto p=salida+d::salidaVelocidad+intentados*sizeof(float);
        if(!compararSeguro(p,v.antes[intentados],v.despues[intentados])) break;
    }
    if(intentados==3) return true;
    // Se revierte únicamente un valor que todavía pertenece a esta invocación.
    while(intentados) {
        --intentados;
        if(!compararSeguro(salida+d::salidaVelocidad+intentados*sizeof(float),v.despues[intentados],v.antes[intentados])) fallo.store(true);
    }
    return false;
}
template<class Clasificar=AnimacionesPublicadas>
uintptr_t despachar(VelocidadNativa original,Direccion base,Direccion retorno,bool permitido,
                   float factor,float ordenVertical,float rapidez,void* metodo,const void* parametros,void* salida,
                   Clasificar clasificar={}) {
    static thread_local bool dentro=false;
    Contexto antes{};float dt=0;
    if(dentro || !permitido || !llamadaPublicada(base,retorno) || !parametros || !salida ||
       !leer(reinterpret_cast<Direccion>(parametros)+d::parametrosDelta,dt) || !std::isfinite(dt) || dt<=0 || dt>1 ||
       !capturar(base,reinterpret_cast<Direccion>(metodo),antes,clasificar)) return original(metodo,parametros,salida);
    struct Salir {bool& b;~Salir(){b=false;}} salir{dentro};dentro=true;
    const auto lugar=reinterpret_cast<Direccion>(salida);
    Velocidad previa{};
    // Compensación de realimentación publicada: devolver X/Z entrantes a su escala
    // nativa antes de calcular la salida nueva evita acelerar exponencialmente.
    if(!preparar(lugar,factor,0,0,true,previa) || !publicar(lugar,previa)) return original(metodo,parametros,salida);
    const auto resultado=original(metodo,parametros,salida);
    Contexto despues{};float dtDespues=0;Velocidad v{};
    if(leer(reinterpret_cast<Direccion>(parametros)+d::parametrosDelta,dtDespues) && dtDespues==dt &&
       capturar(base,reinterpret_cast<Direccion>(metodo),despues,clasificar) && despues==antes &&
       preparar(lugar,factor,ordenVertical,rapidez,false,v) &&
       capturar(base,reinterpret_cast<Direccion>(metodo),despues,clasificar) && despues==antes && publicar(lugar,v))
        ultimaAplicacion.store(GetTickCount64());
    return resultado;
}
uintptr_t __fastcall velocidadHook(void* metodo,const void* parametros,void* salida) {
#if defined(_MSC_VER)
    const auto retorno=reinterpret_cast<Direccion>(_ReturnAddress());
#else
    const auto retorno=reinterpret_cast<Direccion>(__builtin_return_address(0));
#endif
    const auto ahora=GetTickCount64(), entrada=ultimoInput.load();
    const bool usar=habilitado.load() && !fallo.load() && boton.load() && entrada && ahora>=entrada && ahora-entrada<=500;
    const float f=1+(factorElegido.load()-1)*adelante.load();
    return despachar(velocidadOriginal.load(std::memory_order_acquire),baseJuego,retorno,usar,f,vertical.load(),rapidezElegida.load(),metodo,parametros,salida);
}
bool hashCorrecto() {
    wchar_t ruta[32768]{};
    const DWORD longitud=GetModuleFileNameW(nullptr,ruta,32768);
    if(!longitud || longitud>=32768) return false;
    HANDLE archivo=CreateFileW(ruta,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(archivo==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algoritmo{};BCRYPT_HASH_HANDLE hash{};bool igual=false;
    if(BCryptOpenAlgorithmProvider(&algoritmo,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
       BCryptCreateHash(algoritmo,&hash,nullptr,0,nullptr,0,0)>=0) {
        std::vector<unsigned char> bytes(1u<<20);DWORD n=0;bool valido=true;
        for(;;) {
            if(!ReadFile(archivo,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr)) {valido=false;break;}
            if(!n) break;
            if(BCryptHashData(hash,bytes.data(),n,0)<0) {valido=false;break;}
        }
        unsigned char resumen[32]{};
        if(valido && BCryptFinishHash(hash,resumen,32,0)>=0) {
            char texto[65]{};
            for(size_t i=0;i<32;++i) std::snprintf(texto+2*i,3,"%02x",resumen[i]);
            igual=std::strcmp(texto,d::kExeHash)==0;
        }
    }
    if(hash) BCryptDestroyHash(hash);
    if(algoritmo) BCryptCloseAlgorithmProvider(algoritmo,0);
    CloseHandle(archivo);return igual;
}
bool perfilCorrecto(Direccion base) {
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!leer(base,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<=0 || dos.e_lfanew>1024*1024 ||
       !leer(base+dos.e_lfanew,nt) || nt.Signature!=IMAGE_NT_SIGNATURE ||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.FileHeader.TimeDateStamp!=d::marcaTiempoPE ||
       nt.OptionalHeader.SizeOfImage!=d::tamanoImagenPE || !hashCorrecto()) return false;
    for(const auto& pin:d::kPins) {
        if(pin.rva>nt.OptionalHeader.SizeOfImage || pin.length>nt.OptionalHeader.SizeOfImage-pin.rva) return false;
        std::vector<uint8_t> bytes(pin.length);
        if(!leer(base+pin.rva,bytes.data(),bytes.size()) || std::memcmp(bytes.data(),pin.bytes,pin.length)) return false;
    }
    return true;
}
}
bool iniciarPlaneoNativo() {
    if(conectado.load()) return true;
    baseJuego=reinterpret_cast<Direccion>(GetModuleHandleW(nullptr));
    if(!perfilCorrecto(baseJuego)) {motivo="no disponible: el EXE o los testigos de planeo no coinciden con el perfil público 2976";return false;}
    const auto mh=MH_Initialize();
    if(mh!=MH_OK && mh!=MH_ERROR_ALREADY_INITIALIZED) {motivo="no disponible: no se pudo inicializar el hook nativo de planeo";return false;}
    const auto objetivo=baseJuego+d::kSpeedRva;void* trampoline=nullptr;
    if(!ejecutable(objetivo) || MH_CreateHook(reinterpret_cast<void*>(objetivo),reinterpret_cast<void*>(&velocidadHook),&trampoline)!=MH_OK) return false;
    if(!trampoline) {MH_RemoveHook(reinterpret_cast<void*>(objetivo));return false;}
    velocidadOriginal.store(reinterpret_cast<VelocidadNativa>(trampoline),std::memory_order_release);
    if(MH_EnableHook(reinterpret_cast<void*>(objetivo))!=MH_OK) {
        // Después de intentar activar un hook, el trampoline permanece vivo.
        MH_DisableHook(reinterpret_cast<void*>(objetivo));motivo="no disponible: falló la activación del hook de planeo";return false;
    }
    conectado.store(true);return true;
}
bool planeoNativoDisponible(){return conectado.load() && !fallo.load();}
const char* estadoPlaneoNativo(){
    if(fallo.load()) return "no disponible: una publicación de movimiento perdió su identidad; reinicia el juego";
    if(!conectado.load()) return motivo;
    if(!habilitado.load()) return "Planeo nativo 2976 disponible; controles desactivados";
    const auto ahora=GetTickCount64(), ultima=ultimaAplicacion.load();
    return ultima && ahora>=ultima && ahora-ultima<=1500 ? "Planeo real reconocido; velocidad y controles verticales aplicados" :
        "Activado; esperando planeo real, entrada reciente y botón configurado";
}
void activarPlaneoNativo(bool activar){habilitado.store(activar && planeoNativoDisponible());if(!activar) boton.store(false);}
bool planeoNativoActivo(){return habilitado.load() && planeoNativoDisponible();}
void controlesPlaneoNativo(bool mantener,float a,float v,float f,float rapidez){
    if(!std::isfinite(a) || !std::isfinite(v) || !std::isfinite(f) || !std::isfinite(rapidez)) {boton.store(false);return;}
    adelante.store(std::clamp(a,0.0f,1.0f));vertical.store(std::clamp(v,-1.0f,1.0f));
    factorElegido.store(std::clamp(f,1.0f,5.0f));rapidezElegida.store(std::clamp(rapidez,0.0f,50.0f));
    ultimoInput.store(GetTickCount64());boton.store(mantener);
}
}
