#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
#include "render.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace ds {
namespace {
using Microsoft::WRL::ComPtr;
using Presentar = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Presentar1 = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using Redimensionar = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Redimensionar1 = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using Ejecutar = void (STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using Crear = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using CrearHwnd = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using CrearCore = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, IUnknown*,
    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
using CrearComposicion = HRESULT (STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*,
    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);

Presentar originalPresentar{};
Presentar1 originalPresentar1{};
Redimensionar originalRedimensionar{};
Redimensionar1 originalRedimensionar1{};
Ejecutar originalEjecutar{};
Crear originalCrear{};
CrearHwnd originalCrearHwnd{};
CrearCore originalCrearCore{};
CrearComposicion originalCrearComposicion{};
DibujarMenu dibujarMenu{};
AvisarRender avisar{};
ConsultarMenu consultarMenu{};
std::atomic_bool disponible{false};
std::atomic_bool iniciado{false};
std::mutex mutexMapeo;
std::recursive_mutex mutexRender;

struct Relacion {
    // La identidad se compara; nunca se desreferencia después de Release.
    IUnknown* identidad{};
    ComPtr<ID3D12CommandQueue> cola;
};
std::vector<Relacion> relaciones;
thread_local bool dentroDePresentar = false;
thread_local bool dentroDeRedimensionar = false;

struct GuardiaReentrada {
    bool& bandera;
    explicit GuardiaReentrada(bool& valor) : bandera(valor) { bandera = true; }
    ~GuardiaReentrada() { bandera = false; }
};

struct Cuadro {
    ComPtr<ID3D12Resource> buffer;
    ComPtr<ID3D12CommandAllocator> asignador;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 ultimaCerca{};
};

struct EstadoRender {
    ComPtr<IDXGISwapChain3> cadena;
    ComPtr<ID3D12Device> dispositivo;
    ComPtr<ID3D12CommandQueue> cola;
    ComPtr<ID3D12DescriptorHeap> rtv;
    ComPtr<ID3D12DescriptorHeap> srv;
    ComPtr<ID3D12GraphicsCommandList> lista;
    ComPtr<ID3D12Fence> cerca;
    std::vector<Cuadro> cuadros;
    HWND ventana{};
    WNDPROC wndAnterior{};
    HANDLE evento{};
    UINT64 siguienteCerca{1};
    bool win32Listo{};
    bool dx12Listo{};
    bool contextoListo{};
    bool suspendido{};
    bool falloGpu{};
};
EstadoRender estado;

void aviso(const char* texto) noexcept {
    try { if (avisar) avisar(texto); }
    catch (...) { /* Un error del registro no debe salir hacia el juego. */ }
}

IUnknown* identidadDe(IUnknown* objeto) {
    if (!objeto) return nullptr;
    IUnknown* identidad{};
    if (FAILED(objeto->QueryInterface(IID_PPV_ARGS(&identidad)))) return nullptr;
    IUnknown* resultado = identidad;
    identidad->Release();
    return resultado;
}

void registrarCadena(IUnknown* colaDesconocida, IDXGISwapChain* cadena) noexcept {
    try {
        if (!colaDesconocida || !cadena) return;
        ComPtr<ID3D12CommandQueue> cola;
        if (FAILED(colaDesconocida->QueryInterface(IID_PPV_ARGS(&cola))) ||
            cola->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return;
        auto identidad = identidadDe(cadena);
        if (!identidad) return;
        std::lock_guard bloqueo(mutexMapeo);
        for (auto& relacion : relaciones) {
            if (relacion.identidad == identidad) { relacion.cola = cola; return; }
        }
        if (relaciones.size() >= 32) {
            aviso("Render: demasiadas cadenas gráficas; se omite la nueva cadena.");
            return;
        }
        relaciones.push_back({identidad, cola});
    } catch (...) {
        aviso("Render: no se pudo guardar la relación entre cola y cadena; se omite el menú de esa ventana.");
    }
}

ComPtr<ID3D12CommandQueue> buscarCola(IDXGISwapChain* cadena) {
    auto identidad = identidadDe(cadena);
    std::lock_guard bloqueo(mutexMapeo);
    for (auto& relacion : relaciones)
        if (relacion.identidad == identidad) return relacion.cola;
    return {};
}

HRESULT STDMETHODCALLTYPE crearCadena(IDXGIFactory* fabrica, IUnknown* dispositivo,
    DXGI_SWAP_CHAIN_DESC* descripcion, IDXGISwapChain** salida) {
    const auto resultado = originalCrear(fabrica, dispositivo, descripcion, salida);
    if (SUCCEEDED(resultado) && salida && *salida) registrarCadena(dispositivo, *salida);
    return resultado;
}
HRESULT STDMETHODCALLTYPE crearCadenaHwnd(IDXGIFactory2* fabrica, IUnknown* dispositivo, HWND ventana,
    const DXGI_SWAP_CHAIN_DESC1* descripcion, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* completa,
    IDXGIOutput* salidaPantalla, IDXGISwapChain1** salida) {
    const auto resultado = originalCrearHwnd(fabrica, dispositivo, ventana, descripcion, completa, salidaPantalla, salida);
    if (SUCCEEDED(resultado) && salida && *salida) registrarCadena(dispositivo, *salida);
    return resultado;
}
HRESULT STDMETHODCALLTYPE crearCadenaCore(IDXGIFactory2* fabrica, IUnknown* dispositivo, IUnknown* ventana,
    const DXGI_SWAP_CHAIN_DESC1* descripcion, IDXGIOutput* salidaPantalla, IDXGISwapChain1** salida) {
    const auto resultado = originalCrearCore(fabrica, dispositivo, ventana, descripcion, salidaPantalla, salida);
    if (SUCCEEDED(resultado) && salida && *salida) registrarCadena(dispositivo, *salida);
    return resultado;
}
HRESULT STDMETHODCALLTYPE crearCadenaComposicion(IDXGIFactory2* fabrica, IUnknown* dispositivo,
    const DXGI_SWAP_CHAIN_DESC1* descripcion, IDXGIOutput* salidaPantalla, IDXGISwapChain1** salida) {
    const auto resultado = originalCrearComposicion(fabrica, dispositivo, descripcion, salidaPantalla, salida);
    if (SUCCEEDED(resultado) && salida && *salida) registrarCadena(dispositivo, *salida);
    return resultado;
}

bool esTecla(UINT mensaje) {
    return mensaje == WM_KEYDOWN || mensaje == WM_SYSKEYDOWN || mensaje == WM_CHAR ||
        mensaje == WM_SYSCHAR || mensaje == WM_UNICHAR;
}
bool esMouse(UINT mensaje) {
    return mensaje == WM_MOUSEMOVE || mensaje == WM_MOUSEWHEEL || mensaje == WM_MOUSEHWHEEL ||
        mensaje == WM_LBUTTONDOWN || mensaje == WM_LBUTTONDBLCLK ||
        mensaje == WM_RBUTTONDOWN || mensaje == WM_RBUTTONDBLCLK ||
        mensaje == WM_MBUTTONDOWN || mensaje == WM_MBUTTONDBLCLK ||
        mensaje == WM_XBUTTONDOWN || mensaje == WM_XBUTTONDBLCLK;
}

LRESULT CALLBACK procesarVentana(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam) {
    WNDPROC anterior{};
    bool capturado = false;
    {
        std::lock_guard bloqueo(mutexRender);
        anterior = estado.wndAnterior;
        try {
            // Los eventos de liberación llegan al juego para evitar teclas atascadas.
            if (estado.contextoListo && !estado.suspendido && !estado.falloGpu) {
                ImGui_ImplWin32_WndProcHandler(ventana, mensaje, wparam, lparam);
                if (consultarMenu && consultarMenu()) {
                    const auto& io = ImGui::GetIO();
                    capturado = (io.WantCaptureKeyboard && esTecla(mensaje)) || (io.WantCaptureMouse && esMouse(mensaje));
                }
            }
        } catch (...) {
            estado.suspendido = true;
            disponible = false;
            aviso("Render: menú suspendido por una excepción al procesar la entrada.");
        }
    }
    // Nunca retiene el mutex al entrar al procedimiento del juego.
    if (capturado) return 1;
    return anterior ? CallWindowProcW(anterior, ventana, mensaje, wparam, lparam) :
        DefWindowProcW(ventana, mensaje, wparam, lparam);
}

bool esperarCerca(UINT64 valor, DWORD limiteMs = 100) {
    if (!valor || !estado.cerca || estado.cerca->GetCompletedValue() >= valor) return true;
    if (!estado.evento || FAILED(estado.cerca->SetEventOnCompletion(valor, estado.evento))) return false;
    return WaitForSingleObject(estado.evento, limiteMs) == WAIT_OBJECT_0;
}

bool esperarAntesDeRedimensionar() {
    if(!estado.cola || !estado.cerca || !estado.dispositivo) return true;
    // La cerca se coloca DESPUÉS de todos los comandos ya enviados por el menú.
    // No se inventa un HRESULT de DXGI porque un GPU sano tarde más de 100 ms.
    const auto valor=estado.siguienteCerca++;
    bool avisado=false;
    while(FAILED(estado.cola->Signal(estado.cerca.Get(),valor))) {
        if(FAILED(estado.dispositivo->GetDeviceRemovedReason())) return false;
        if(!avisado) {aviso("Render: esperando sincronización antes de cambiar resolución.");avisado=true;}
        Sleep(10);
    }
    while(estado.cerca->GetCompletedValue()<valor) {
        if(FAILED(estado.dispositivo->GetDeviceRemovedReason())) return false;
        if(estado.evento && SUCCEEDED(estado.cerca->SetEventOnCompletion(valor,estado.evento)))
            WaitForSingleObject(estado.evento,50);
        else Sleep(10);
    }
    return SUCCEEDED(estado.dispositivo->GetDeviceRemovedReason());
}

void liberarBuffers() {
    disponible = false;
    if (estado.dx12Listo) {
        ImGui_ImplDX12_Shutdown();
        estado.dx12Listo = false;
    }
    estado.cuadros.clear();
    estado.lista.Reset();
    estado.rtv.Reset();
    estado.srv.Reset();
}

bool prepararBuffers() {
    DXGI_SWAP_CHAIN_DESC descripcion{};
    if (!estado.cadena || FAILED(estado.cadena->GetDesc(&descripcion)) ||
        descripcion.BufferCount < 2 || descripcion.BufferCount > 16) return false;
    auto formato = descripcion.BufferDesc.Format;
    if (formato == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) formato = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (formato == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) formato = DXGI_FORMAT_B8G8R8A8_UNORM;
    D3D12_DESCRIPTOR_HEAP_DESC heapRtv{};
    heapRtv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapRtv.NumDescriptors = descripcion.BufferCount;
    if (FAILED(estado.dispositivo->CreateDescriptorHeap(&heapRtv, IID_PPV_ARGS(&estado.rtv)))) return false;
    D3D12_DESCRIPTOR_HEAP_DESC heapSrv{};
    heapSrv.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapSrv.NumDescriptors = 1;
    heapSrv.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(estado.dispositivo->CreateDescriptorHeap(&heapSrv, IID_PPV_ARGS(&estado.srv)))) return false;
    estado.cuadros.resize(descripcion.BufferCount);
    const auto paso = estado.dispositivo->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto handle = estado.rtv->GetCPUDescriptorHandleForHeapStart();
    for (UINT indice = 0; indice < descripcion.BufferCount; ++indice) {
        auto& cuadro = estado.cuadros[indice];
        if (FAILED(estado.cadena->GetBuffer(indice, IID_PPV_ARGS(&cuadro.buffer))) ||
            FAILED(estado.dispositivo->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cuadro.asignador)))) return false;
        cuadro.rtv = handle;
        estado.dispositivo->CreateRenderTargetView(cuadro.buffer.Get(), nullptr, cuadro.rtv);
        handle.ptr += paso;
    }
    if (FAILED(estado.dispositivo->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        estado.cuadros[0].asignador.Get(), nullptr, IID_PPV_ARGS(&estado.lista))) || FAILED(estado.lista->Close())) return false;
    estado.dx12Listo = ImGui_ImplDX12_Init(estado.dispositivo.Get(), static_cast<int>(descripcion.BufferCount), formato,
        estado.srv.Get(), estado.srv->GetCPUDescriptorHandleForHeapStart(), estado.srv->GetGPUDescriptorHandleForHeapStart());
    if (!estado.dx12Listo) return false;
    disponible = true;
    return true;
}

bool prepararBuffersConResguardo() noexcept {
    try { return prepararBuffers(); }
    catch (...) {
        aviso("Render: no se pudieron volver a crear los buffers del menú.");
        return false;
    }
}

bool preparar(IDXGISwapChain* cadena) {
    auto cola = buscarCola(cadena);
    if (!cola) {
        static std::atomic_bool avisado{false};
        if (!avisado.exchange(true)) aviso("Render: aún no se observó la creación de la cadena DX12. Reinicia el juego con el ASI loader activo desde el inicio.");
        return false;
    }
    ComPtr<ID3D12Device> dispositivoCadena;
    ComPtr<ID3D12Device> dispositivoCola;
    if (FAILED(cadena->GetDevice(IID_PPV_ARGS(&dispositivoCadena))) ||
        FAILED(cola->GetDevice(IID_PPV_ARGS(&dispositivoCola))) ||
        identidadDe(dispositivoCadena.Get()) != identidadDe(dispositivoCola.Get())) return false;
    DXGI_SWAP_CHAIN_DESC descripcion{};
    if (FAILED(cadena->GetDesc(&descripcion)) || !descripcion.OutputWindow || !IsWindow(descripcion.OutputWindow)) return false;
    DWORD proceso{};
    GetWindowThreadProcessId(descripcion.OutputWindow, &proceso);
    if (proceso != GetCurrentProcessId()) return false;
    // El menú solo toma una ventana visible del proceso; ignora cadenas auxiliares ocultas.
    if (!IsWindowVisible(descripcion.OutputWindow)) return false;
    if (FAILED(cadena->QueryInterface(IID_PPV_ARGS(&estado.cadena)))) return false;
    estado.dispositivo = dispositivoCadena;
    estado.cola = cola;
    estado.ventana = descripcion.OutputWindow;
    if (FAILED(estado.dispositivo->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&estado.cerca)))) return false;
    estado.evento = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!estado.evento) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    estado.contextoListo = true;
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    estado.win32Listo = ImGui_ImplWin32_Init(estado.ventana);
    if (!estado.win32Listo || !prepararBuffers()) return false;
    SetLastError(0);
    estado.wndAnterior = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(estado.ventana, GWLP_WNDPROC));
    const auto anterior = SetWindowLongPtrW(estado.ventana, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&procesarVentana));
    if (!anterior) return false;
    estado.wndAnterior = reinterpret_cast<WNDPROC>(anterior);
    aviso("Render DX12: menú preparado con la cola exacta de la cadena gráfica.");
    return true;
}

void limpiarInicioFallido() {
    liberarBuffers();
    if (estado.win32Listo) ImGui_ImplWin32_Shutdown();
    if (estado.contextoListo) ImGui::DestroyContext();
    if (estado.evento) CloseHandle(estado.evento);
    estado.evento = nullptr;
    estado.win32Listo = false;
    estado.contextoListo = false;
    estado.cadena.Reset();
    estado.dispositivo.Reset();
    estado.cola.Reset();
    estado.cerca.Reset();
}

void dibujarCuadro() {
    if (!disponible || estado.suspendido || estado.falloGpu) return;
    const auto indice = estado.cadena->GetCurrentBackBufferIndex();
    if (indice >= estado.cuadros.size()) return;
    auto& cuadro = estado.cuadros[indice];
    // Si el GPU se atrasa, omite el menú de este cuadro en lugar de esperar sin límite.
    if (!esperarCerca(cuadro.ultimaCerca, 2)) return;
    if (FAILED(cuadro.asignador->Reset()) || FAILED(estado.lista->Reset(cuadro.asignador.Get(), nullptr))) {
        estado.falloGpu = true;
        disponible = false;
        aviso("Render: se desactivó el menú por un error al preparar comandos DX12.");
        return;
    }
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (dibujarMenu) dibujarMenu();
    ImGui::Render();
    D3D12_RESOURCE_BARRIER barrera{};
    barrera.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrera.Transition.pResource = cuadro.buffer.Get();
    barrera.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrera.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrera.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    estado.lista->ResourceBarrier(1, &barrera);
    estado.lista->OMSetRenderTargets(1, &cuadro.rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[]{estado.srv.Get()};
    estado.lista->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), estado.lista.Get());
    barrera.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrera.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    estado.lista->ResourceBarrier(1, &barrera);
    if (FAILED(estado.lista->Close())) { estado.falloGpu = true; disponible = false; return; }
    ID3D12CommandList* listas[]{estado.lista.Get()};
    // Llama al original: evita recursión y conserva el orden de la cola del juego.
    originalEjecutar(estado.cola.Get(), 1, listas);
    const auto valor = estado.siguienteCerca++;
    if (FAILED(estado.cola->Signal(estado.cerca.Get(), valor))) {
        // Conserva los recursos: el GPU podría seguir usándolos.
        estado.falloGpu = true;
        disponible = false;
        aviso("Render: se suspendió el menú porque el GPU rechazó la sincronización.");
        return;
    }
    cuadro.ultimaCerca = valor;
}

void antesDePresentar(IDXGISwapChain* cadena) noexcept {
    try {
        std::lock_guard bloqueo(mutexRender);
        if (!estado.suspendido && !estado.falloGpu) {
            if (!estado.cadena) {
                if (!preparar(cadena) && estado.cadena) {
                    limpiarInicioFallido();
                    estado.suspendido = true;
                    aviso("Render: la inicialización falló; el juego continúa sin menú.");
                }
            }
            if (estado.cadena && identidadDe(estado.cadena.Get()) == identidadDe(cadena)) dibujarCuadro();
        }
    } catch (...) {
        // Una excepción de C++ del menú nunca debe desenrollar la pila del motor.
        std::lock_guard bloqueo(mutexRender);
        estado.suspendido = true;
        disponible = false;
        aviso("Render: menú suspendido por una excepción de C++; reinicia para volver a intentarlo.");
    }
}

void despuesDePresentar(HRESULT resultado) {
    if (resultado == DXGI_ERROR_DEVICE_REMOVED || resultado == DXGI_ERROR_DEVICE_RESET) {
        std::lock_guard bloqueo(mutexRender);
        estado.falloGpu = true;
        disponible = false;
    }
}

HRESULT STDMETHODCALLTYPE presentar(IDXGISwapChain* cadena, UINT sincronizacion, UINT banderas) {
    if (dentroDePresentar || (banderas & DXGI_PRESENT_TEST)) return originalPresentar(cadena, sincronizacion, banderas);
    GuardiaReentrada guardia(dentroDePresentar);
    antesDePresentar(cadena);
    const auto resultado = originalPresentar(cadena, sincronizacion, banderas);
    despuesDePresentar(resultado);
    return resultado;
}

HRESULT STDMETHODCALLTYPE presentar1(IDXGISwapChain1* cadena, UINT sincronizacion, UINT banderas,
    const DXGI_PRESENT_PARAMETERS* parametros) {
    if (dentroDePresentar || (banderas & DXGI_PRESENT_TEST)) return originalPresentar1(cadena, sincronizacion, banderas, parametros);
    GuardiaReentrada guardia(dentroDePresentar);
    antesDePresentar(cadena);
    const auto resultado = originalPresentar1(cadena, sincronizacion, banderas, parametros);
    despuesDePresentar(resultado);
    return resultado;
}

HRESULT STDMETHODCALLTYPE redimensionar(IDXGISwapChain* cadena, UINT cantidad, UINT ancho, UINT alto,
    DXGI_FORMAT formato, UINT banderas) {
    if (dentroDeRedimensionar) return originalRedimensionar(cadena, cantidad, ancho, alto, formato, banderas);
    std::unique_lock bloqueo(mutexRender);
    if (!estado.cadena || identidadDe(estado.cadena.Get()) != identidadDe(cadena)) {
        bloqueo.unlock();
        return originalRedimensionar(cadena, cantidad, ancho, alto, formato, banderas);
    }
    if(!esperarAntesDeRedimensionar()) {
        estado.falloGpu=true;
        aviso("Render: el dispositivo reportó pérdida. Se conserva el resultado real de ResizeBuffers del juego.");
    }
    liberarBuffers();
    const bool suspendidoAntes = estado.suspendido;
    estado.suspendido = true;
    GuardiaReentrada guardia(dentroDeRedimensionar);
    bloqueo.unlock();
    const auto resultado = originalRedimensionar(cadena, cantidad, ancho, alto, formato, banderas);
    bloqueo.lock();
    estado.suspendido = suspendidoAntes;
    if (FAILED(resultado) || (!estado.suspendido && (estado.falloGpu || !prepararBuffersConResguardo()))) {
        estado.suspendido = true;
        disponible = false;
        aviso("Render: menú suspendido después de un cambio de resolución.");
    }
    return resultado;
}

HRESULT STDMETHODCALLTYPE redimensionar1(IDXGISwapChain3* cadena, UINT cantidad, UINT ancho, UINT alto,
    DXGI_FORMAT formato, UINT banderas, const UINT* nodos, IUnknown* const* colas) {
    if (dentroDeRedimensionar) return originalRedimensionar1(cadena, cantidad, ancho, alto, formato, banderas, nodos, colas);
    std::unique_lock bloqueo(mutexRender);
    if (!estado.cadena || identidadDe(estado.cadena.Get()) != identidadDe(cadena)) {
        bloqueo.unlock();
        return originalRedimensionar1(cadena, cantidad, ancho, alto, formato, banderas, nodos, colas);
    }
    if(!esperarAntesDeRedimensionar()) {
        estado.falloGpu=true;
        aviso("Render: el dispositivo reportó pérdida. Se conserva el resultado real de ResizeBuffers1 del juego.");
    }
    liberarBuffers();
    const bool suspendidoAntes = estado.suspendido;
    estado.suspendido = true;
    GuardiaReentrada guardia(dentroDeRedimensionar);
    bloqueo.unlock();
    const auto resultado = originalRedimensionar1(cadena, cantidad, ancho, alto, formato, banderas, nodos, colas);
    bloqueo.lock();
    estado.suspendido = suspendidoAntes;
    if (SUCCEEDED(resultado) && colas) {
        DXGI_SWAP_CHAIN_DESC descripcion{};
        bool iguales = SUCCEEDED(cadena->GetDesc(&descripcion)) && descripcion.BufferCount > 0;
        const auto primera = iguales ? identidadDe(colas[0]) : nullptr;
        for (UINT i = 0; iguales && i < descripcion.BufferCount; ++i)
            iguales = primera && identidadDe(colas[i]) == primera;
        if (iguales) {
            ComPtr<ID3D12CommandQueue> nuevaCola;
            if (SUCCEEDED(colas[0]->QueryInterface(IID_PPV_ARGS(&nuevaCola))) &&
                nuevaCola->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                estado.cola = nuevaCola;
                registrarCadena(nuevaCola.Get(), cadena);
            } else iguales = false;
        }
        if (!iguales) {
            estado.suspendido = true;
            aviso("Render: las colas de presentación cambiaron; el menú se suspendió.");
        }
    }
    if (FAILED(resultado) || (!estado.suspendido && (estado.falloGpu || !prepararBuffersConResguardo()))) {
        estado.suspendido = true;
        disponible = false;
        aviso("Render: menú suspendido después de un cambio de resolución DXGI 1.4.");
    }
    return resultado;
}

void STDMETHODCALLTYPE ejecutar(ID3D12CommandQueue* cola, UINT cantidad, ID3D12CommandList* const* listas) {
    // El mapeo proviene de DXGI, nunca de elegir la primera cola que aparece.
    originalEjecutar(cola, cantidad, listas);
}

LRESULT CALLBACK ventanaPrueba(HWND ventana, UINT mensaje, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcW(ventana, mensaje, wparam, lparam);
}

struct ObjetivoHook { void* objetivo{}; void* reemplazo{}; void** original{}; };
}

bool renderDisponible() { return disponible.load(); }

bool iniciarRender(DibujarMenu dibujar, AvisarRender registrar, ConsultarMenu visible) {
    if (iniciado.exchange(true)) return false;
    dibujarMenu = dibujar;
    avisar = registrar;
    consultarMenu = visible;
    const auto modulo = GetModuleHandleW(nullptr);
    constexpr wchar_t clase[] = L"DesiertoSeguro_VentanaPrueba_DX12";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ventanaPrueba;
    wc.hInstance = modulo;
    wc.lpszClassName = clase;
    if (!RegisterClassExW(&wc)) { aviso("Render: no se pudo registrar la ventana de prueba."); return false; }
    HWND ventana = CreateWindowExW(0, clase, L"Prueba DX12", WS_OVERLAPPEDWINDOW,
        0, 0, 64, 64, nullptr, nullptr, modulo, nullptr);
    if (!ventana) { UnregisterClassW(clase, modulo); return false; }
    ComPtr<IDXGIFactory4> fabrica;
    ComPtr<ID3D12Device> dispositivo;
    ComPtr<ID3D12CommandQueue> cola;
    ComPtr<IDXGISwapChain1> cadena;
    ComPtr<IDXGISwapChain3> cadena3;
    D3D12_COMMAND_QUEUE_DESC descripcionCola{};
    descripcionCola.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    DXGI_SWAP_CHAIN_DESC1 descripcionCadena{};
    descripcionCadena.Width = 64;
    descripcionCadena.Height = 64;
    descripcionCadena.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    descripcionCadena.SampleDesc.Count = 1;
    descripcionCadena.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    descripcionCadena.BufferCount = 2;
    descripcionCadena.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    bool correcto = SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&fabrica))) &&
        SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dispositivo))) &&
        SUCCEEDED(dispositivo->CreateCommandQueue(&descripcionCola, IID_PPV_ARGS(&cola))) &&
        SUCCEEDED(fabrica->CreateSwapChainForHwnd(cola.Get(), ventana, &descripcionCadena, nullptr, nullptr, &cadena)) &&
        SUCCEEDED(cadena.As(&cadena3));
    std::vector<void*> creados;
    if (correcto) {
        auto vCadena = *reinterpret_cast<void***>(cadena.Get());
        auto vCadena3 = *reinterpret_cast<void***>(cadena3.Get());
        auto vCola = *reinterpret_cast<void***>(cola.Get());
        auto vFabrica = *reinterpret_cast<void***>(fabrica.Get());
        // Estos índices pertenecen a interfaces COM públicas de Microsoft, no al juego.
        std::array<ObjetivoHook, 9> objetivos{{
            {vFabrica[10], reinterpret_cast<void*>(&crearCadena), reinterpret_cast<void**>(&originalCrear)},
            {vFabrica[15], reinterpret_cast<void*>(&crearCadenaHwnd), reinterpret_cast<void**>(&originalCrearHwnd)},
            {vFabrica[16], reinterpret_cast<void*>(&crearCadenaCore), reinterpret_cast<void**>(&originalCrearCore)},
            {vFabrica[24], reinterpret_cast<void*>(&crearCadenaComposicion), reinterpret_cast<void**>(&originalCrearComposicion)},
            {vCola[10], reinterpret_cast<void*>(&ejecutar), reinterpret_cast<void**>(&originalEjecutar)},
            {vCadena[13], reinterpret_cast<void*>(&redimensionar), reinterpret_cast<void**>(&originalRedimensionar)},
            {vCadena3[39], reinterpret_cast<void*>(&redimensionar1), reinterpret_cast<void**>(&originalRedimensionar1)},
            {vCadena[22], reinterpret_cast<void*>(&presentar1), reinterpret_cast<void**>(&originalPresentar1)},
            {vCadena[8], reinterpret_cast<void*>(&presentar), reinterpret_cast<void**>(&originalPresentar)}
        }};
        const auto inicializacion = MH_Initialize();
        correcto = inicializacion == MH_OK || inicializacion == MH_ERROR_ALREADY_INITIALIZED;
        if (correcto) {
            for (auto& objetivo : objetivos) {
                if (MH_CreateHook(objetivo.objetivo, objetivo.reemplazo, objetivo.original) != MH_OK) { correcto = false; break; }
                creados.push_back(objetivo.objetivo);
            }
        }
        if (correcto) {
            // Activa únicamente nuestros hooks y publica el lote al terminar.
            for (auto objetivo : creados) if (MH_QueueEnableHook(objetivo) != MH_OK) { correcto = false; break; }
            if (correcto && MH_ApplyQueued() != MH_OK) correcto = false;
        }
        if (!correcto) {
            for (auto objetivo : creados) { MH_DisableHook(objetivo); MH_RemoveHook(objetivo); }
        }
    }
    cadena3.Reset();
    cadena.Reset();
    cola.Reset();
    dispositivo.Reset();
    fabrica.Reset();
    DestroyWindow(ventana);
    UnregisterClassW(clase, modulo);
    aviso(correcto ? "Render: hooks DX12 instalados; esperando la ventana del juego." :
        "Render: no se instalaron los hooks DX12; revisa el log y otros overlays.");
    return correcto;
}
}
