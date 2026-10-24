#include "clima.hpp"
#include "datos_juego.hpp"
#include "memoria.hpp"
#include <Windows.h>
#include <MinHook.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>
#ifdef _MSC_VER
#include <intrin.h>
#define DS_RETORNO_CLIMA reinterpret_cast<uintptr_t>(_ReturnAddress())
#else
#define DS_RETORNO_CLIMA reinterpret_cast<uintptr_t>(__builtin_return_address(0))
#endif

// Adaptación de las fuentes MIT de crimson-workbench, commit
// 0c86f9be5afdc007b4e9e8c3c93b12ac46837462, weather_native.h,
// weather_control.h y world_mod.cpp. Los datos viven en datos_juego.hpp.
namespace ds {
namespace {
namespace dc = datos::clima;
using ProductorClima = void(__fastcall*)(void*, float);
using ConsumidorClima = void(__fastcall*)(void*);
using SetterClima = void(__fastcall*)(void*, const float*);
using SetterManual = void(__fastcall*)(void*, const std::uint8_t*);
using DerivarClima = void(__fastcall*)(void*);
struct EnlacesClima {
    SetterManual manual{};
    SetterClima precipitacion{}, nubosidad{}, humedad{}, viento{}, niebla{}, densidad{};
    DerivarClima derivar{};
} enlaces;
std::atomic<ProductorClima> originalPrincipal{}, originalSecundario{};
std::atomic<ConsumidorClima> originalCliente{};
std::atomic_bool conectado{false}, solicitado{false}, disponible{false}, fallo{false};
std::atomic_int estado{0};
std::atomic<std::uint64_t> ultimoCliente{0};
uintptr_t baseClima = 0;
std::mutex mutexSolicitud;
AjustesClima seleccion;
std::atomic_flag enProceso = ATOMIC_FLAG_INIT;
thread_local bool dentroClima = false;

struct IdentidadClima {
    uintptr_t administrador{}, recurso{}, constante{}, atmosfera{}, mundo{};
    bool operator==(const IdentidadClima&) const = default;
};
struct MarcoClima { IdentidadClima identidad; bool manual{}; };
struct InstantaneaClima {
    std::array<std::uint8_t, dc::tamanoEscalares> escalares{};
    float aerosol{}, niebla{}, nubes{};
    bool operator==(const InstantaneaClima&) const = default;
};
struct TestigoClima {
    IdentidadClima identidad;
    std::uint32_t mundo{}, mascara{};
    std::uint16_t etapa = dc::etapaInvalida;
    std::uint64_t instante{}, generacion{};
} testigo;
// Estos valores solo se acceden dentro de los posthooks serializados.
IdentidadClima identidadPropia;
IdentidadClima identidadEsperando;
InstantaneaClima original;
AjustesClima ultimoAplicado;
bool poseido = false, manualOriginal = false, esperarAutomatico = false;
std::uint64_t generacionPropia = 0;
std::uint64_t generacionEsperando = 0;

struct EntradaClima {
    bool entrada;
    EntradaClima() : entrada(!dentroClima) { if (entrada) dentroClima = true; }
    ~EntradaClima() { if (entrada) dentroClima = false; }
};
struct ExclusivaClima {
    bool tomada = !enProceso.test_and_set(std::memory_order_acquire);
    ~ExclusivaClima() { if (tomada) enProceso.clear(std::memory_order_release); }
};
bool ajustesValidos(const AjustesClima& a) {
    return std::isfinite(a.precipitacion) && a.precipitacion >= 0 && a.precipitacion <= 1 &&
        std::isfinite(a.nubosidad) && a.nubosidad >= -1 && a.nubosidad <= 1 &&
        std::isfinite(a.humedad) && a.humedad >= 0 && a.humedad <= 1 &&
        std::isfinite(a.viento) && a.viento >= 0 && a.viento <= 60 &&
        std::isfinite(a.niebla) && a.niebla >= 0 && a.niebla <= 10;
}
bool tablaValida(uintptr_t objeto, uintptr_t tabla) {
    uintptr_t valor = 0;
    return objeto && leer(objeto, valor) && valor == baseClima + tabla;
}
bool abrirMarco(uintptr_t administrador, MarcoClima& marco) {
    marco = {};
    uintptr_t servicio = 0, intermedio = 0, actual = 0, mundo = 0, enlace = 0;
    uintptr_t resultado = 0, constante = 0, atmosfera = 0, padre = 0;
    std::uint8_t manual = 2;
    if (!tablaValida(administrador, dc::tablaAdministrador) ||
        !leer(baseClima + dc::servicio, servicio) || !servicio ||
        !leer(servicio + dc::servicioIntermedio, intermedio) || !intermedio ||
        !leer(intermedio + dc::intermedioAdministrador, actual) || actual != administrador ||
        !leer(administrador + dc::administradorMundo, mundo) || !tablaValida(mundo, dc::tablaMundo) ||
        !leer(mundo + dc::mundoAdministrador, enlace) || enlace != administrador ||
        !leer(administrador + dc::administradorResultado, resultado) || resultado < dc::ajusteInterfaz ||
        !tablaValida(resultado - dc::ajusteInterfaz, dc::tablaRecurso)) return false;
    const uintptr_t recurso = resultado - dc::ajusteInterfaz;
    if (!leer(recurso + dc::recursoClima, constante) || constante < dc::ajusteInterfaz ||
        !leer(recurso + dc::recursoAtmosfera, atmosfera) || atmosfera < dc::ajusteInterfaz ||
        !tablaValida(constante - dc::ajusteInterfaz, dc::tablaClima) ||
        !tablaValida(atmosfera - dc::ajusteInterfaz, dc::tablaAtmosfera) ||
        !leer(constante - dc::hijoPadre, padre) || padre != recurso ||
        !leer(atmosfera - dc::hijoPadre, padre) || padre != recurso ||
        !leer(administrador + dc::manual, manual) || manual > 1) return false;
    marco.identidad = {administrador, recurso, constante, atmosfera, mundo};
    marco.manual = manual != 0;
    return true;
}
bool leerInstantanea(const MarcoClima& marco, InstantaneaClima& copia, AjustesClima& a) {
    copia = {};
    const auto& i = marco.identidad;
    for (const auto& segmento : dc::segmentos)
        if (!leer(i.constante + segmento.dato, copia.escalares.data() + segmento.copia, segmento.longitud)) return false;
    if (!leer(i.atmosfera + dc::atmosferaAerosol, copia.aerosol) || !std::isfinite(copia.aerosol) ||
        !leer(i.atmosfera + dc::atmosferaNiebla, copia.niebla) || !std::isfinite(copia.niebla) ||
        !leer(i.atmosfera + dc::atmosferaNubes, copia.nubes) || !std::isfinite(copia.nubes)) return false;
    for (std::size_t n = 0; n < copia.escalares.size(); n += sizeof(float)) {
        // Las banderas y sus huecos no son números flotantes ni memoria restaurable.
        if (n == dc::byteBandera || (n >= dc::inicioBanderas && n < dc::finBanderas)) continue;
        float valor = 0;
        std::memcpy(&valor, copia.escalares.data() + n, sizeof(valor));
        if (!std::isfinite(valor)) return false;
    }
    float fase = 0;
    return leer(i.constante + dc::climaPrecipitacion, a.precipitacion) &&
        leer(i.constante + dc::climaNubosidad, a.nubosidad) &&
        leer(i.constante + dc::climaHumedad, a.humedad) &&
        leer(i.constante + dc::climaViento, a.viento) &&
        leer(i.constante + dc::climaFase, fase) &&
        ((a.niebla = copia.niebla), (a.nieve = fase <= 0), true);
}
bool escribirInstantanea(const MarcoClima& marco, const InstantaneaClima& copia) {
    const auto& i = marco.identidad;
    for (const auto& segmento : dc::segmentos)
        if (!escribirDatos(i.constante + segmento.dato, copia.escalares.data() + segmento.copia, segmento.longitud)) return false;
    return escribir(i.atmosfera + dc::atmosferaAerosol, copia.aerosol) &&
        escribir(i.atmosfera + dc::atmosferaNiebla, copia.niebla) &&
        escribir(i.atmosfera + dc::atmosferaNubes, copia.nubes);
}
bool restaurarInstantanea(const MarcoClima& marco, const InstantaneaClima& copia) {
    InstantaneaClima previa, observada;
    AjustesClima ignorados;
    if (!leerInstantanea(marco, previa, ignorados)) return false;
    if (escribirInstantanea(marco, copia) && leerInstantanea(marco, observada, ignorados) && observada == copia) return true;
    escribirInstantanea(marco, previa);
    return false;
}
// Ningún objeto con destructor participa en estas envolturas SEH de llamadas nativas.
bool ponerManual(const MarcoClima& marco, bool activar) {
    const std::uint8_t valor = activar ? 1 : 0;
    __try {
        enlaces.manual(reinterpret_cast<void*>(marco.identidad.administrador), &valor);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    std::uint8_t observado = 2;
    return leer(marco.identidad.administrador + dc::manual, observado) && observado == valor;
}
bool invocarAjustes(const MarcoClima& marco, const AjustesClima& a) {
    const auto& i = marco.identidad;
    const auto primaria = reinterpret_cast<void*>(i.constante - dc::ajusteInterfaz);
    const auto atmosfera = reinterpret_cast<void*>(i.atmosfera - dc::ajusteInterfaz);
    const float nubes = a.nubosidad > 0 ? a.nubosidad : 0;
    __try {
        enlaces.precipitacion(primaria, &a.precipitacion);
        enlaces.nubosidad(primaria, &a.nubosidad);
        enlaces.humedad(primaria, &a.humedad);
        enlaces.viento(primaria, &a.viento);
        enlaces.niebla(atmosfera, &a.niebla);
        enlaces.densidad(atmosfera, &nubes);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    const float cero = 0, fase = a.nieve ? -1.0f : 1.0f;
    if (!escribir(i.atmosfera + dc::atmosferaAerosol, cero) || !escribir(i.constante + dc::climaFase, fase)) return false;
    __try {
        enlaces.derivar(reinterpret_cast<void*>(i.recurso));
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
bool siguenPropios(const InstantaneaClima& copia, const AjustesClima& actual) {
    // Los productores originales actualizan la fase y otras salidas derivadas.
    // Solo comparamos las entradas que poseemos, igual que la fuente fijada.
    return actual.precipitacion == ultimoAplicado.precipitacion && actual.nubosidad == ultimoAplicado.nubosidad &&
        actual.humedad == ultimoAplicado.humedad && actual.viento == ultimoAplicado.viento &&
        copia.niebla == ultimoAplicado.niebla && copia.nubes == (ultimoAplicado.nubosidad > 0 ? ultimoAplicado.nubosidad : 0) && copia.aerosol == 0;
}
bool aplicarAjustes(const MarcoClima& marco, const AjustesClima& a) {
    if (!ajustesValidos(a) || !invocarAjustes(marco, a)) return false;
    InstantaneaClima copia;
    AjustesClima observado;
    float lluvia = -1, nieve = -1;
    ultimoAplicado = a;
    return leerInstantanea(marco, copia, observado) && siguenPropios(copia, observado) && observado.nieve == a.nieve &&
        leer(marco.identidad.constante + dc::climaLluvia, lluvia) && lluvia == (a.nieve ? 0 : a.precipitacion) &&
        leer(marco.identidad.constante + dc::climaNieve, nieve) && nieve == (a.nieve ? a.precipitacion : 0);
}
void soltarPropiedad() {
    poseido = false;
    identidadPropia = {};
    solicitado.store(false);
}
void bloquearClima() {
    fallo.store(true);
    disponible.store(false);
    solicitado.store(false);
    estado.store(5);
}
void procesarClima(uintptr_t administrador, uintptr_t retorno, std::uint64_t ahora, bool principal) {
    if (fallo.load()) return;
    if (retorno != baseClima + (principal ? dc::retornoPrincipal : dc::retornoSecundario)) return;
    MarcoClima marco;
    if (!abrirMarco(administrador, marco)) { disponible.store(false); return; }
    InstantaneaClima actual;
    AjustesClima parametros;
    if (!leerInstantanea(marco, actual, parametros)) { bloquearClima(); return; }
    const bool contexto = testigo.instante && ahora >= testigo.instante && ahora - testigo.instante <= dc::vigenciaClienteMs && testigo.identidad == marco.identidad;
    disponible.store(contexto);
    if (poseido && !(identidadPropia == marco.identidad)) {
        // Se abandona el objeto anterior sin leerlo: su vida pudo terminar.
        soltarPropiedad(); estado.store(6); return;
    }
    if (poseido && (!marco.manual || !siguenPropios(actual, parametros))) {
        soltarPropiedad(); estado.store(6); return;
    }
    if (poseido && (!solicitado.load() || !contexto || generacionPropia != testigo.generacion)) {
        if (!restaurarInstantanea(marco, original) || !ponerManual(marco, manualOriginal)) { bloquearClima(); return; }
        poseido = false; identidadPropia = {}; solicitado.store(false);
        esperarAutomatico = !manualOriginal;
        identidadEsperando = esperarAutomatico ? marco.identidad : IdentidadClima{};
        generacionEsperando = testigo.generacion;
        estado.store(esperarAutomatico ? 7 : 2);
        return;
    }
    if (esperarAutomatico) {
        if (!(identidadEsperando == marco.identidad) || generacionEsperando != testigo.generacion) {
            // La espera pertenece al productor liberado, nunca al siguiente mundo.
            esperarAutomatico = false; identidadEsperando = {}; estado.store(2);
        } else {
            if (principal && !marco.manual) { esperarAutomatico = false; identidadEsperando = {}; estado.store(2); }
            return;
        }
    }
    if (!contexto || !solicitado.load()) { estado.store(contexto ? 2 : 1); return; }
    AjustesClima deseado = ultimoAplicado;
    {
        std::unique_lock<std::mutex> cierre(mutexSolicitud, std::try_to_lock);
        // Mantener las salidas poseídas es necesario después de cada productor,
        // incluso mientras el menú publica su siguiente selección.
        if (cierre.owns_lock()) deseado = seleccion;
        else if (!poseido) return;
    }
    if (!poseido) {
        original = actual; manualOriginal = marco.manual; identidadPropia = marco.identidad;
        generacionPropia = testigo.generacion;
        if (!ponerManual(marco, true)) { bloquearClima(); return; }
        poseido = true;
    }
    if (!aplicarAjustes(marco, deseado)) { bloquearClima(); return; }
    // La confirmación final la proporciona el consumidor nativo del campo.
    estado.store(3);
}
void observarCliente(uintptr_t campo, uintptr_t retorno, std::uint64_t ahora) {
    if (fallo.load() || retorno != baseClima + dc::retornoCliente) return;
    // Una lectura inválida revoca inmediatamente el testigo previo del campo.
    // Se conserva la generación para poder reconocer la siguiente transición.
    disponible.store(false);
    ultimoCliente.store(0);
    const auto instanteAnterior = testigo.instante;
    testigo.instante = 0;
    if (!tablaValida(campo, dc::tablaCampo)) return;
    uintptr_t buffers = 0, activo = 0, servicio = 0, intermedio = 0, administrador = 0;
    std::uint8_t fase = dc::cantidadFases, enJuego = 0;
    std::uint32_t mundo = 0, mascara = 0;
    std::uint16_t etapa = dc::etapaInvalida;
    MarcoClima marco;
    if (!leer(campo + dc::campoBuffers, buffers) || !buffers || !leer(buffers + dc::buffersFase, fase) || fase >= dc::cantidadFases ||
        !leer(buffers + dc::buffersEntradas + fase * sizeof(uintptr_t), activo) || !activo ||
        !leer(activo + dc::bufferActivo, enJuego) || !enJuego ||
        !leer(baseClima + dc::servicio, servicio) || !servicio || !leer(servicio + dc::servicioIntermedio, intermedio) || !intermedio ||
        !leer(intermedio + dc::intermedioAdministrador, administrador) || !abrirMarco(administrador, marco) ||
        !leer(campo + dc::campoMundo, mundo) || !leer(campo + dc::campoEtapa, etapa) || etapa == dc::etapaInvalida ||
        !leer(campo + dc::campoMascara, mascara)) return;
    const bool cambio = (instanteAnterior || poseido || esperarAutomatico) &&
        (testigo.mundo != mundo || testigo.etapa != etapa || !(testigo.identidad == marco.identidad));
    if (cambio) ++testigo.generacion;
    testigo.identidad = marco.identidad; testigo.mundo = mundo; testigo.etapa = etapa;
    testigo.mascara = mascara; testigo.instante = ahora; ultimoCliente.store(ahora);
    disponible.store(true);
    if (poseido && testigo.generacion == generacionPropia && identidadPropia == marco.identidad) {
        const std::uint32_t esperado = ultimoAplicado.precipitacion > 0 ?
            (ultimoAplicado.nieve ? dc::mascaraNieve : dc::mascaraLluvia) : dc::mascaraSol;
        if ((mascara & dc::mascaraPrecipitacionSol) == esperado) estado.store(4);
    }
}
void __fastcall hookPrincipal(void* administrador, float delta) {
    const auto llamada = originalPrincipal.load(std::memory_order_acquire);
    if (!llamada) return;
    EntradaClima entrada;
    const uintptr_t retorno = DS_RETORNO_CLIMA;
    llamada(administrador, delta);
    if (!entrada.entrada || !conectado.load()) return;
    ExclusivaClima cierre;
    if (cierre.tomada) procesarClima(reinterpret_cast<uintptr_t>(administrador), retorno, GetTickCount64(), true);
}
void __fastcall hookSecundario(void* administrador, float delta) {
    const auto llamada = originalSecundario.load(std::memory_order_acquire);
    if (!llamada) return;
    EntradaClima entrada;
    const uintptr_t retorno = DS_RETORNO_CLIMA;
    llamada(administrador, delta);
    if (!entrada.entrada || !conectado.load()) return;
    ExclusivaClima cierre;
    if (cierre.tomada) procesarClima(reinterpret_cast<uintptr_t>(administrador), retorno, GetTickCount64(), false);
}
void __fastcall hookCliente(void* campo) {
    const auto llamada = originalCliente.load(std::memory_order_acquire);
    if (!llamada) return;
    EntradaClima entrada;
    const uintptr_t retorno = DS_RETORNO_CLIMA;
    llamada(campo);
    if (!entrada.entrada || !conectado.load()) return;
    ExclusivaClima cierre;
    if (cierre.tomada) observarCliente(reinterpret_cast<uintptr_t>(campo), retorno, GetTickCount64());
}
bool verificarHash(const wchar_t* ruta) {
    HANDLE archivo = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (archivo == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algoritmo{}; BCRYPT_HASH_HANDLE hash{};
    bool coincide = false;
    if (BCryptOpenAlgorithmProvider(&algoritmo, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algoritmo, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        std::vector<unsigned char> buffer(1u << 20);
        DWORD cantidad = 0;
        bool correcto = true;
        for (;;) {
            if (!ReadFile(archivo, buffer.data(), static_cast<DWORD>(buffer.size()), &cantidad, nullptr)) { correcto = false; break; }
            if (!cantidad) break;
            if (BCryptHashData(hash, buffer.data(), cantidad, 0) < 0) { correcto = false; break; }
        }
        unsigned char resumen[32]{};
        if (correcto && BCryptFinishHash(hash, resumen, sizeof(resumen), 0) >= 0) {
            constexpr char digitos[] = "0123456789abcdef";
            char texto[65]{};
            for (unsigned n = 0; n < sizeof(resumen); ++n) { texto[n * 2] = digitos[resumen[n] >> 4]; texto[n * 2 + 1] = digitos[resumen[n] & 15]; }
            coincide = std::strcmp(texto, dc::kWorldExeHash) == 0;
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algoritmo) BCryptCloseAlgorithmProvider(algoritmo, 0);
    CloseHandle(archivo);
    return coincide;
}
bool verificarImagen() {
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if (!leer(baseClima, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 || dos.e_lfanew > 0x1000 ||
        !leer(baseClima + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.FileHeader.TimeDateStamp != dc::selloPE || nt.OptionalHeader.SizeOfImage != dc::tamanoPE) return false;
    for (const auto& pin : dc::kWorldCodePins) {
        std::vector<unsigned char> observado(pin.length);
        if (!leer(baseClima + pin.rva, observado.data(), observado.size()) || std::memcmp(observado.data(), pin.bytes, pin.length)) return false;
    }
    for (const auto& pin : dc::kWorldTablePins) {
        uintptr_t observado = 0;
        if (!leer(baseClima + pin.slot, observado) || observado != baseClima + pin.target) return false;
    }
    return true;
}
}

bool iniciarClima() {
    if (conectado.load()) return true;
    // Un intento de instalación por proceso; todos los trampolines sobreviven a la activación.
    static std::atomic_bool intentado{false};
    if (intentado.exchange(true)) return false;
    wchar_t ruta[32768]{};
    baseClima = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!baseClima || !GetModuleFileNameW(nullptr, ruta, 32768) || !verificarHash(ruta) || !verificarImagen()) return false;
    enlaces = {reinterpret_cast<SetterManual>(baseClima + dc::setterManual),
        reinterpret_cast<SetterClima>(baseClima + dc::setterPrecipitacion),
        reinterpret_cast<SetterClima>(baseClima + dc::setterNubosidad),
        reinterpret_cast<SetterClima>(baseClima + dc::setterHumedad),
        reinterpret_cast<SetterClima>(baseClima + dc::setterViento),
        reinterpret_cast<SetterClima>(baseClima + dc::setterNiebla),
        reinterpret_cast<SetterClima>(baseClima + dc::setterDensidad),
        reinterpret_cast<DerivarClima>(baseClima + dc::derivarPrecipitacion)};
    HMODULE fijado{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&iniciarClima), &fijado)) return false;
    const auto inicializado = MH_Initialize();
    if (inicializado != MH_OK && inicializado != MH_ERROR_ALREADY_INITIALIZED) return false;
    const uintptr_t destinos[] = {baseClima + dc::productorPrincipal, baseClima + dc::productorSecundario, baseClima + dc::consumidorCliente};
    void* callbacks[] = {reinterpret_cast<void*>(&hookPrincipal), reinterpret_cast<void*>(&hookSecundario), reinterpret_cast<void*>(&hookCliente)};
    void* trampolines[3]{};
    unsigned creados = 0;
    for (; creados < 3; ++creados) {
        if (!ejecutable(destinos[creados]) || MH_CreateHook(reinterpret_cast<void*>(destinos[creados]), callbacks[creados], &trampolines[creados]) != MH_OK) break;
        if (!trampolines[creados]) { ++creados; break; }
    }
    if (creados != 3 || !trampolines[0] || !trampolines[1] || !trampolines[2]) {
        while (creados) MH_RemoveHook(reinterpret_cast<void*>(destinos[--creados]));
        return false;
    }
    originalPrincipal.store(reinterpret_cast<ProductorClima>(trampolines[0]), std::memory_order_release);
    originalSecundario.store(reinterpret_cast<ProductorClima>(trampolines[1]), std::memory_order_release);
    originalCliente.store(reinterpret_cast<ConsumidorClima>(trampolines[2]), std::memory_order_release);
    for (unsigned n = 0; n < 3; ++n) {
        if (MH_EnableHook(reinterpret_cast<void*>(destinos[n])) != MH_OK) {
            for (unsigned anterior = 0; anterior <= n; ++anterior) MH_DisableHook(reinterpret_cast<void*>(destinos[anterior]));
            // Nunca se eliminan trampolines después de un intento de activación.
            return false;
        }
    }
    conectado.store(true);
    estado.store(1);
    return true;
}
bool climaDisponible() {
    const auto ahora = GetTickCount64(), ultimo = ultimoCliente.load();
    return conectado.load() && !fallo.load() && disponible.load() && ultimo && ahora >= ultimo && ahora - ultimo <= dc::vigenciaClienteMs;
}
const char* estadoClima() {
    if (!conectado.load()) return "Clima no disponible: el ejecutable no coincide con el original publicado 2976";
    if (solicitado.load() && !fallo.load() && !climaDisponible())
        return "Control del clima solicitado; esperando un mundo y un campo cliente activos";
    switch (estado.load()) {
    case 2: return "Clima automático; parámetros nativos disponibles";
    case 3: return "Parámetros aplicados; esperando confirmación del campo";
    case 4: return "Clima confirmado por el consumidor nativo (sin prueba visual en juego)";
    case 5: return "Clima desactivado: falló una validación o publicación nativa";
    case 6: return "Control abandonado: cambió el mundo, recurso o una entrada ajena";
    case 7: return "Estado anterior restaurado; esperando productor automático";
    default: return "Esperando un mundo y un campo cliente activos";
    }
}
void activarClima(bool activar) { solicitado.store(activar && conectado.load() && !fallo.load()); }
bool climaActivo() { return solicitado.load() && conectado.load() && !fallo.load(); }
AjustesClima ajustesClima() { std::lock_guard<std::mutex> cierre(mutexSolicitud); return seleccion; }
void fijarClima(const AjustesClima& ajustes) {
    if (!ajustesValidos(ajustes)) return;
    std::lock_guard<std::mutex> cierre(mutexSolicitud);
    seleccion = ajustes;
}
void actualizarClima() {
    if (conectado.load() && !climaDisponible() && !fallo.load()) disponible.store(false);
}
}
