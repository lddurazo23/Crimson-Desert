#include <Windows.h>
#include <cwchar>
#include "configuracion.hpp"
#include "funciones.hpp"
#include "menu.hpp"
#include "render.hpp"

namespace {
DWORD WINAPI iniciar(void*) {
    wchar_t ruta[32768]{};
    if(!GetModuleFileNameW(nullptr,ruta,32768)) return 0;
    const wchar_t* nombre=wcsrchr(ruta,L'\\');
    nombre=nombre ? nombre+1 : ruta;
    if(_wcsicmp(nombre,L"CrimsonDesert.exe")!=0) return 0;
    // Los detours viven hasta cerrar el juego. Se impide descargar la DLL con hilos activos.
    HMODULE fijado{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&iniciar),&fijado);
    ds::cargarConfiguracion();
    ds::registrar("DesiertoSeguro: inicio. Configuración y log en LOCALAPPDATA/DesiertoSeguro.");
    // Primero DXGI: hay que observar la creación de la swapchain del juego.
    if(!ds::iniciarRender(ds::dibujarMenu,ds::registrar,ds::menuVisible)) {
        ds::registrar("Se omitieron todas las funciones porque el menú no pudo instalarse.");
        return 0;
    }
    // El loader entra antes que el mundo. Esperar al primer renderer evita escanear
    // código que todavía no terminó de cargarse, sin detener el hilo principal del juego.
    const ULONGLONG comienzo=GetTickCount64();
    while(!ds::renderDisponible() && GetTickCount64()-comienzo<180000) Sleep(25);
    if(!ds::renderDisponible()) {
        ds::registrar("No se observó una presentación DX12 utilizable; no se instalaron funciones del juego.");
        return 0;
    }
    try { ds::iniciarFunciones(); }
    catch(...) { ds::notificarFalloInicio(); ds::registrar("La inicialización de funciones falló; revisa el registro. No se activarán nuevas operaciones."); }
    return 0;
}
}
BOOL APIENTRY DllMain(HMODULE modulo,DWORD motivo,LPVOID) {
    if(motivo==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(modulo);
        if(HANDLE hilo=CreateThread(nullptr,0,iniciar,nullptr,0,nullptr)) CloseHandle(hilo);
    }
    return TRUE;
}
