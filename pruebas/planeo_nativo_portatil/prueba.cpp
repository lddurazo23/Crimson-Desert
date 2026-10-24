// Datos sintéticos: estas animaciones NO pertenecen al juego ni completan marcadores.
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#include "planeo_nativo.cpp"

uint64_t tiempoSimulado=2000;
uintptr_t baseSimulada=0x100000000ull;
namespace {
struct Rango {uintptr_t direccion;const unsigned char* bytes;size_t largo;};
std::vector<Rango> rangos;
uintptr_t denegar=0;unsigned intercambios=0,denegarIntento=0;
unsigned llamadas=0;bool cambiarActor=false;
std::array<float,3> entradaRecibida{},salidaNativa{20,-2,4};
uintptr_t parametroRecibido=0,metodoRecibido=0,salidaRecibida=0,actorDePrueba=0;
template<class T> void poner(uintptr_t p,const T& v){std::memcpy(reinterpret_cast<void*>(p),&v,sizeof(v));}
template<class T,size_t N> void poner(std::array<uint8_t,N>& b,size_t o,const T& v){assert(o+sizeof(v)<=N);std::memcpy(b.data()+o,&v,sizeof(v));}
template<class T> void agregar(T& b){rangos.push_back({reinterpret_cast<uintptr_t>(&b),reinterpret_cast<unsigned char*>(&b),sizeof(b)});}
const Rango* rango(uintptr_t p,size_t n){
    for(const auto& r:rangos) if(p>=r.direccion && p-r.direccion<=r.largo && n<=r.largo-(p-r.direccion)) return &r;
    return nullptr;
}
struct ClasificarPrueba {
    uint64_t huella;uint8_t eventos,capa;
    bool operator()(uint64_t h,uint8_t e,uint8_t c)const{return h==huella && e==eventos && c==capa;}
};
struct Fixture {
    std::array<uint8_t,0x80> raiz{},administrador{},metodo{};
    std::array<uint8_t,0x200> actor{},control{},estado{},paquete{};
    std::array<uint8_t,0xe0> posesor{};
    std::array<uint8_t,0x60> metadatos{},componentes{},selector{},capa{},animacion{},vista{};
    std::array<uint8_t,156> fija{};
    std::array<uint8_t,0x90> parametros{},salida{};
    uintptr_t global=0;
    ClasificarPrueba clasificador{};
    Fixture(){
        rangos.clear();denegar=0;denegarIntento=0;intercambios=0;llamadas=0;cambiarActor=false;
        using namespace ds::datos::planeoNativo;
        agregar(raiz);agregar(administrador);agregar(metodo);agregar(actor);agregar(control);agregar(estado);
        agregar(paquete);agregar(posesor);agregar(metadatos);agregar(componentes);agregar(selector);agregar(capa);
        agregar(animacion);agregar(vista);agregar(fija);agregar(parametros);agregar(salida);
        global=reinterpret_cast<uintptr_t>(raiz.data());
        rangos.push_back({baseSimulada+raizJugador,reinterpret_cast<unsigned char*>(&global),sizeof(global)});
        const auto ptr=[](const auto& x){return reinterpret_cast<uintptr_t>(x.data());};
        poner(metodo,0,baseSimulada+vtMetodo);poner(metodo,metodoControl,ptr(control));poner(metodo,metodoActor,ptr(actor));
        poner(raiz,raizAdministrador,ptr(administrador));poner(administrador,0,baseSimulada+vtAdministrador);
        poner(administrador,administradorActor,ptr(actor));poner(actor,0,baseSimulada+vtActor);
        poner(actor,actorVivo,uint8_t(1));poner(actor,actorIdentidad,uint32_t(27));poner(actor,actorMetadatos,ptr(metadatos));
        poner(actor,actorComponentes,ptr(componentes));poner(actor,actorCantidadComponentes,uint32_t(9));
        poner(actor,actorPosesor,ptr(posesor));poner(posesor,posesorActor,ptr(actor));poner(metadatos,metadatosTipo,uint8_t(1));
        poner(componentes,componentesControl,ptr(control));poner(control,0,baseSimulada+vtControl);
        poner(control,controlActor,ptr(actor));poner(control,controlEstado,ptr(estado));poner(control,controlIntegrador,uintptr_t(0x12345678));
        poner(estado,estadoControl,ptr(control));poner(estado,estadoActor,ptr(actor));poner(estado,estadoModo,uint8_t(0));
        poner(estado,estadoMetodos,ptr(metodo));poner(control,controlSelector,ptr(selector));poner(selector,selectorCapa,uint8_t(1));
        poner(control,controlCapas+8,ptr(capa));poner(capa,capaAnimacion,ptr(animacion));poner(animacion,animacionFija,ptr(fija));
        poner(animacion,animacionPaquete,ptr(paquete));poner(animacion,animacionVista,ptr(vista));poner(animacion,animacionCondiciones,uintptr_t(12345));
        poner(paquete,paqueteEventos,eventosInferior);poner(paquete,paqueteCondiciones,condicionesInferior);poner(paquete,paqueteBuscadores,buscadoresInferior);
        poner(paquete,paqueteBuffer,uintptr_t(23456));poner(vista,vistaCantidadEventos,uint8_t(11));poner(vista,vistaInstancias,uintptr_t(34567));
        for(size_t i=0;i<fija.size();++i)fija[i]=static_cast<uint8_t>(i*13+7);
        uint64_t h=14695981039346656037ull;for(auto b:fija)h=(h^b)*1099511628211ull;
        clasificador={h,11,1};poner(parametros,parametrosDelta,1.0f/60);
        valores({30,-1,6});actorDePrueba=ptr(actor);ds::fallo.store(false);
    }
    void valores(std::array<float,3> v){std::memcpy(salida.data()+ds::datos::planeoNativo::salidaVelocidad,v.data(),sizeof(v));}
    std::array<float,3> valores()const{std::array<float,3> v{};std::memcpy(v.data(),salida.data()+ds::datos::planeoNativo::salidaVelocidad,sizeof(v));return v;}
    uintptr_t direccionMetodo()const{return reinterpret_cast<uintptr_t>(metodo.data());}
};
uintptr_t __fastcall original(void* metodo,const void* params,void* salida){
    ++llamadas;metodoRecibido=reinterpret_cast<uintptr_t>(metodo);parametroRecibido=reinterpret_cast<uintptr_t>(params);
    salidaRecibida=reinterpret_cast<uintptr_t>(salida);
    std::memcpy(entradaRecibida.data(),reinterpret_cast<uint8_t*>(salida)+ds::datos::planeoNativo::salidaVelocidad,sizeof(entradaRecibida));
    std::memcpy(reinterpret_cast<uint8_t*>(salida)+ds::datos::planeoNativo::salidaVelocidad,salidaNativa.data(),sizeof(salidaNativa));
    if(cambiarActor) poner(actorDePrueba+ds::datos::planeoNativo::actorVivo,uint8_t(0));
    return 0xfedcba9876543210ull;
}
uintptr_t despachar(Fixture& f,bool usar=true,float factor=2.5f,float v=0,float rapidez=15,uintptr_t retorno=0){
    return ds::despachar(&original,baseSimulada,retorno ? retorno:baseSimulada+ds::datos::planeoNativo::kReturns[0],
        usar,factor,v,rapidez,f.metodo.data(),f.parametros.data(),f.salida.data(),f.clasificador);
}
}
namespace ds {
bool leer(uintptr_t p,void* dst,size_t n){const auto* r=rango(p,n);if(!r)return false;std::memcpy(dst,r->bytes+p-r->direccion,n);return true;}
bool ejecutable(uintptr_t){return false;}
}
size_t VirtualQuery(const void* p,MEMORY_BASIC_INFORMATION* m,size_t){
    const auto* r=rango(reinterpret_cast<uintptr_t>(p),4);if(!r)return 0;
    m->BaseAddress=reinterpret_cast<void*>(r->direccion);m->RegionSize=r->largo;m->State=MEM_COMMIT;m->Protect=PAGE_READWRITE;return sizeof(*m);
}
LONG InterlockedCompareExchange(volatile LONG* p,LONG nuevo,LONG antes){
    ++intercambios;uint32_t actual=0;std::memcpy(&actual,const_cast<const LONG*>(p),sizeof(actual));
    if(reinterpret_cast<uintptr_t>(p)==denegar || (denegarIntento && intercambios==denegarIntento)) return static_cast<LONG>(actual^0x123u);
    if(actual==static_cast<uint32_t>(antes)) std::memcpy(const_cast<LONG*>(p),&nuevo,sizeof(nuevo));
    return static_cast<LONG>(actual);
}
MH_STATUS MH_Initialize(){return MH_OK;}
MH_STATUS MH_CreateHook(void*,void*,void**){return MH_ERROR_UNKNOWN;}
MH_STATUS MH_EnableHook(void*){return MH_OK;}
MH_STATUS MH_DisableHook(void*){return MH_OK;}
MH_STATUS MH_RemoveHook(void*){return MH_OK;}

int main(){
    using namespace ds::datos::planeoNativo;
    {
        Fixture f;ds::Contexto c{};assert(ds::capturar(baseSimulada,f.direccionMetodo(),c,f.clasificador));
        assert(!ds::capturar(baseSimulada,f.direccionMetodo(),c));
        assert(despachar(f)==0xfedcba9876543210ull && llamadas==1);
        assert((entradaRecibida==std::array<float,3>{12,-1,2.4f}));
        assert((f.valores()==std::array<float,3>{50,-2,10}));
        assert(metodoRecibido==f.direccionMetodo() && parametroRecibido==reinterpret_cast<uintptr_t>(f.parametros.data()));
        assert(salidaRecibida==reinterpret_cast<uintptr_t>(f.salida.data()));
    }
    for(float v:{-1.0f,1.0f}){Fixture f;despachar(f,true,2,v,20);assert(f.valores()[1]==v*20);}
    {
        Fixture f;despachar(f,false);assert(intercambios==0 && f.valores()==salidaNativa);
        llamadas=0;despachar(f,true,2.5f,0,15,baseSimulada+1);assert(llamadas==1 && intercambios==0);
    }
    {
        Fixture f;poner(f.selector,selectorCapa,uint8_t(0));despachar(f);assert(intercambios==0);
        poner(f.control,controlCapas,reinterpret_cast<uintptr_t>(f.capa.data()));
        poner(f.paquete,paqueteEventos,eventosSuperior);poner(f.paquete,paqueteCondiciones,condicionesSuperior);poner(f.paquete,paqueteBuscadores,buscadoresSuperior);
        f.clasificador.capa=0;despachar(f,true,2,1,13);assert((f.valores()==std::array<float,3>{40,13,8}));
    }
    {
        Fixture f;cambiarActor=true;despachar(f);assert(f.valores()==salidaNativa && intercambios==3);
    }
    {
        Fixture f;denegarIntento=5;despachar(f);assert(f.valores()==salidaNativa && !ds::fallo.load());
    }
    {
        Fixture f;for(unsigned i=0;i<600;++i){despachar(f);assert((f.valores()==std::array<float,3>{50,-2,10}));}
        assert(llamadas==600);
    }
    {
        Fixture f;poner(f.metadatos,metadatosTipo,uint8_t(3));despachar(f);assert(intercambios==0);
        poner(f.metadatos,metadatosTipo,uint8_t(4));despachar(f);assert(intercambios==6);
    }
    {
        Fixture f;poner(f.parametros,parametrosDelta,std::nanf(""));despachar(f);assert(intercambios==0);
        assert(!ds::iniciarPlaneoNativo() && !ds::planeoNativoDisponible());
    }
    {
        Fixture f;ds::Contexto c{};
        for(size_t i=0;i<f.fija.size();++i){f.fija[i]^=1;assert(!ds::capturar(baseSimulada,f.direccionMetodo(),c,f.clasificador));f.fija[i]^=1;}
        assert(ds::capturar(baseSimulada,f.direccionMetodo(),c,f.clasificador));
    }
    {
        Fixture f;poner(f.parametros,parametrosDelta,1.0f/60);
        despachar(f,true,1,0,0);assert(f.valores()==salidaNativa);
        ds::controlesPlaneoNativo(true,1,1,3,20);assert(ds::boton.load());
        ds::controlesPlaneoNativo(true,std::nanf(""),1,3,20);assert(!ds::boton.load());
    }
    ds::AnimacionesPublicadas publicadas;
    for(const auto& a:kAnimations)assert(publicadas(a.fingerprint,a.events,1) && !publicadas(a.fingerprint,a.events,0));
    for(const auto& a:kUpperAnimations)assert(publicadas(a.fingerprint,a.events,0) && !publicadas(a.fingerprint,a.events,1));
    std::cout<<"Planeo nativo: identidad, capas, huellas, controles verticales, compensación, publicación y rechazo de EXE aprobados (simulación).\n";
}
