#include "configuracion.hpp"
#include "teclas.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <mutex>
#include <sstream>
#include <cstdlib>
#include <cwchar>
#include <iterator>

namespace ds {
namespace {
Configuracion opciones;
std::mutex mutexArchivo;
std::string estado = "Configuración todavía no guardada";
int entero(const wchar_t* clave, int defecto) {
    return static_cast<int>(GetPrivateProfileIntW(L"Preferencias", clave, defecto,
        (carpetaDatos()/L"DesiertoSeguro.ini").c_str()));
}
float real(const wchar_t* clave, float defecto, float minimo, float maximo) {
    wchar_t texto[64]{};
    const auto respaldo = std::to_wstring(defecto);
    GetPrivateProfileStringW(L"Preferencias", clave, respaldo.c_str(), texto, 64,
        (carpetaDatos()/L"DesiertoSeguro.ini").c_str());
    wchar_t* fin = nullptr;
    float valor = std::wcstof(texto, &fin);
    return fin == texto || !std::isfinite(valor) ? defecto : std::clamp(valor,minimo,maximo);
}
int tecla(const wchar_t* clave, int defecto) {
    const int valor=entero(clave,defecto);
    return valor>=8 && valor<=254 ? valor : defecto;
}

}
Configuracion& configuracion() { return opciones; }
std::filesystem::path carpetaDatos() {
    static const auto ruta=[] {
        wchar_t local[32768]{};
        const DWORD longitud=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        if (!longitud || longitud>=32768) return std::filesystem::path{};
        return std::filesystem::path(local)/L"DesiertoSeguro";
    }();
    return ruta;
}
bool cargarConfiguracion() {
    if(carpetaDatos().empty()) { estado="No se encontró LOCALAPPDATA"; return false; }
    std::error_code error;
    std::filesystem::create_directories(carpetaDatos(),error);
    if(error) { estado="No se pudo crear la carpeta de configuración"; return false; }
    opciones.teclaMenu=tecla(L"TeclaMenu",0x24);
    opciones.teclaVuelo=tecla(L"TeclaVuelo",0x75);
    opciones.teclaSubir=tecla(L"TeclaSubir",0x20);
    opciones.teclaBajar=tecla(L"TeclaBajar",0x11);
    const auto teclas=normalizarTeclas({opciones.teclaMenu,opciones.teclaVuelo,opciones.teclaSubir,opciones.teclaBajar});
    opciones.teclaMenu=teclas[0]; opciones.teclaVuelo=teclas[1];
    opciones.teclaSubir=teclas[2]; opciones.teclaBajar=teclas[3];
    opciones.botonVuelo=entero(L"BotonVuelo",0x80);
    constexpr int permitidos[]={0x1000,0x2000,0x4000,0x8000,0x100,0x200,0x40,0x80};
    if(std::find(std::begin(permitidos),std::end(permitidos),opciones.botonVuelo)==std::end(permitidos)) opciones.botonVuelo=0x80;
    opciones.vueloAdelante=real(L"VueloAdelante",2,1,5);
    opciones.vueloVertical=real(L"VueloVertical",15,1,50);
    for(size_t i=0;i<3;++i) opciones.vitales[i]=entero((L"Vital"+std::to_wstring(i)).c_str(),0)==1;
    opciones.vuelo=entero(L"Vuelo",0)==1;
    opciones.inventario=entero(L"Inventario",0)==1;
    opciones.tiempo=entero(L"Tiempo",0)==1;
    opciones.congelado=entero(L"Congelado",0)==1;
    opciones.monturasNivel5=entero(L"MonturasNivel5",0)==1;
    opciones.delitos=entero(L"Delitos",0)==1;
    opciones.refinar=entero(L"Refinar",0)==1;
    opciones.equipoNuevo=entero(L"EquipoNuevo",0)==1;
    opciones.clima=entero(L"Clima",0)==1;
    opciones.ajustesDelClima.precipitacion=real(L"Precipitacion",0,0,1);
    opciones.ajustesDelClima.nubosidad=real(L"Nubosidad",0,-1,1);
    opciones.ajustesDelClima.humedad=real(L"Humedad",0,0,1);
    opciones.ajustesDelClima.viento=real(L"Viento",0,0,60);
    opciones.ajustesDelClima.niebla=real(L"NieblaAtmosferica",0,0,10);
    opciones.ajustesDelClima.nieve=entero(L"Nieve",0)==1;
    opciones.conocimientos=entero(L"Conocimientos",0)==1;
    opciones.regionMapa=std::clamp(entero(L"RegionMapa",0),0,5);
    estado="Configuración cargada";
    return true;
}
bool guardarConfiguracion() {
    std::lock_guard bloqueo(mutexArchivo);
    const auto teclas=normalizarTeclas({opciones.teclaMenu,opciones.teclaVuelo,opciones.teclaSubir,opciones.teclaBajar});
    opciones.teclaMenu=teclas[0]; opciones.teclaVuelo=teclas[1];
    opciones.teclaSubir=teclas[2]; opciones.teclaBajar=teclas[3];
    if(carpetaDatos().empty()) return false;
    const auto temporal=carpetaDatos()/L"DesiertoSeguro.ini.nuevo";
    const auto destino=carpetaDatos()/L"DesiertoSeguro.ini";
    std::ostringstream texto;
    texto<<"; Preferencias de DesiertoSeguro. Se cambian desde el menú.\n[Preferencias]\n";
    texto<<"TeclaMenu="<<opciones.teclaMenu<<"\nTeclaVuelo="<<opciones.teclaVuelo;
    texto<<"\nTeclaSubir="<<opciones.teclaSubir<<"\nTeclaBajar="<<opciones.teclaBajar;
    texto<<"\nBotonVuelo="<<opciones.botonVuelo<<"\nVueloAdelante="<<opciones.vueloAdelante;
    texto<<"\nVueloVertical="<<opciones.vueloVertical;
    for(size_t i=0;i<3;++i) texto<<"\nVital"<<i<<"="<<opciones.vitales[i];
    texto<<"\nVuelo="<<opciones.vuelo<<"\nInventario="<<opciones.inventario;
    texto<<"\nTiempo="<<opciones.tiempo<<"\nCongelado="<<opciones.congelado<<"\n";
    texto<<"MonturasNivel5="<<opciones.monturasNivel5<<"\nDelitos="<<opciones.delitos;
    texto<<"\nRefinar="<<opciones.refinar<<"\nEquipoNuevo="<<opciones.equipoNuevo;
    texto<<"\nClima="<<opciones.clima<<"\nConocimientos="<<opciones.conocimientos;
    texto<<"\nRegionMapa="<<opciones.regionMapa;
    texto<<"\nPrecipitacion="<<opciones.ajustesDelClima.precipitacion;
    texto<<"\nNubosidad="<<opciones.ajustesDelClima.nubosidad;
    texto<<"\nHumedad="<<opciones.ajustesDelClima.humedad;
    texto<<"\nViento="<<opciones.ajustesDelClima.viento;
    texto<<"\nNieblaAtmosferica="<<opciones.ajustesDelClima.niebla;
    texto<<"\nNieve="<<opciones.ajustesDelClima.nieve;
    texto<<'\n';
    { std::ofstream archivo(temporal,std::ios::binary|std::ios::trunc); archivo<<texto.str(); archivo.flush();
      if(!archivo) { estado="Error al escribir preferencias"; return false; } }
    if(!MoveFileExW(temporal.c_str(),destino.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        estado="Error al reemplazar preferencias; se conservó el archivo anterior"; return false;
    }
    estado="Preferencias guardadas";
    return true;
}
const char* estadoGuardado() { return estado.c_str(); }
void registrar(const char* texto) {
    std::lock_guard bloqueo(mutexArchivo);
    OutputDebugStringA(texto); OutputDebugStringA("\n");
    if(carpetaDatos().empty()) return;
    SYSTEMTIME fecha{}; GetLocalTime(&fecha);
    std::ofstream archivo(carpetaDatos()/L"DesiertoSeguro.log",std::ios::app|std::ios::binary);
    archivo<<fecha.wYear<<'-'<<fecha.wMonth<<'-'<<fecha.wDay<<' '<<fecha.wHour<<':'<<fecha.wMinute<<':'<<fecha.wSecond<<" "<<texto<<'\n';
}
}
