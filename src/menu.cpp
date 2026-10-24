#include "menu.hpp"
#include "configuracion.hpp"
#include "funciones.hpp"
#include "vitales.hpp"
#include "mundo.hpp"
#include "movimiento.hpp"
#include "clima.hpp"
#include "equipo.hpp"
#include "equipo_nativo.hpp"
#include "monturas_nivel5.hpp"
#include "delitos.hpp"
#include "regiones.hpp"
#include "motor.hpp"
#include "inventario.hpp"
#include <Windows.h>
#include <Xinput.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <string>

namespace ds {
namespace {
std::atomic_bool abierto{false};
std::array<bool,256> anterior{};
int capturar = 0;
ULONGLONG inicioCaptura = 0;
std::string avisoAccion;
bool tieneFoco() {
    DWORD proceso=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&proceso);
    return proceso==GetCurrentProcessId();
}
std::string nombreTecla(int tecla) {
    if(tecla==VK_HOME) return "INICIO (Home)";
    if(tecla==VK_CONTROL) return "Ctrl";
    if(tecla==VK_SPACE) return "Espacio";
    wchar_t texto[128]{};
    UINT codigo=MapVirtualKeyW(static_cast<UINT>(tecla),MAPVK_VK_TO_VSC);
    if(tecla==VK_HOME || tecla==VK_END || tecla==VK_PRIOR || tecla==VK_NEXT ||
       tecla==VK_LEFT || tecla==VK_RIGHT || tecla==VK_UP || tecla==VK_DOWN || tecla==VK_INSERT || tecla==VK_DELETE) codigo|=0x100;
    if(!GetKeyNameTextW(static_cast<LONG>(codigo<<16),texto,128)) return "Tecla "+std::to_string(tecla);
    const int n=WideCharToMultiByte(CP_UTF8,0,texto,-1,nullptr,0,nullptr,nullptr);
    if(n<1) return "Tecla "+std::to_string(tecla);
    std::string salida(static_cast<size_t>(n),'\0');
    WideCharToMultiByte(CP_UTF8,0,texto,-1,salida.data(),n,nullptr,nullptr);
    salida.pop_back();
    return salida;
}
void preferenciasCambiaron() { guardarConfiguracion(); }
void filaTecla(const char* etiqueta,int cual,int valor) {
    ImGui::PushID(cual);
    ImGui::TextUnformatted(etiqueta); ImGui::SameLine(260);
    if(ImGui::Button(capturar==cual ? "Presiona la tecla..." : nombreTecla(valor).c_str())) {
        capturar=cual; inicioCaptura=GetTickCount64();
    }
    ImGui::PopID();
}
void entradas() {
    std::array<bool,256> actual{};
    for(int i=8;i<255;++i) actual[i]=(GetAsyncKeyState(i)&0x8000)!=0;
    auto& c=configuracion();
    if(!tieneFoco()) {
        ajustarVuelo(false,0,0,c.vueloAdelante,c.vueloVertical);
        anterior=actual;
        return;
    }
    if(capturar && GetTickCount64()-inicioCaptura>200) {
        for(int i=8;i<255;++i) {
            if(!actual[i] || anterior[i]) continue;
            if(i==VK_ESCAPE) { capturar=0; break; }
            if(i==VK_SHIFT || i==VK_LSHIFT || i==VK_RSHIFT || i==VK_LCONTROL || i==VK_RCONTROL ||
               i==VK_LMENU || i==VK_RMENU || i==VK_MENU || i==VK_LWIN || i==VK_RWIN) continue;
            int* destino=capturar==1 ? &c.teclaMenu : capturar==2 ? &c.teclaVuelo : capturar==3 ? &c.teclaSubir : &c.teclaBajar;
            const int otras[]={c.teclaMenu,c.teclaVuelo,c.teclaSubir,c.teclaBajar};
            bool repetida=false;
            for(int j=0;j<4;++j) if(j!=capturar-1 && otras[j]==i) repetida=true;
            if(repetida) { avisoAccion="Esa tecla ya está asignada a otra acción."; break; }
            *destino=i; capturar=0; preferenciasCambiaron(); break;
        }
    } else if(!capturar) {
        if(actual[c.teclaMenu] && !anterior[c.teclaMenu]) abierto=!abierto.load();
        if(actual[VK_ESCAPE] && !anterior[VK_ESCAPE]) abierto=false;
    }
    anterior=actual;
    XINPUT_STATE control{};
    bool hayControl=false;
    for(DWORD indice=0;indice<XUSER_MAX_COUNT && !hayControl;++indice)
        hayControl=XInputGetState(indice,&control)==ERROR_SUCCESS;
    const bool mantenido=actual[c.teclaVuelo] || (hayControl && (control.Gamepad.wButtons & c.botonVuelo));
    const float verticalTeclado=(actual[c.teclaSubir] ? 1.0f : 0.0f)-(actual[c.teclaBajar] ? 1.0f : 0.0f);
    const float verticalControl=hayControl ? (static_cast<float>(control.Gamepad.bRightTrigger)-control.Gamepad.bLeftTrigger)/255.0f : 0.0f;
    const float adelanteControl=hayControl ? std::max(0.0f,static_cast<float>(control.Gamepad.sThumbLY)/32767.0f) : 0.0f;
    ajustarVuelo(!abierto.load() && !capturar && mantenido, std::max(actual['W'] ? 1.0f : 0.0f,adelanteControl),
        std::clamp(verticalTeclado+verticalControl,-1.0f,1.0f),c.vueloAdelante,c.vueloVertical);
}
void jugador() {
    auto& c=configuracion();
    constexpr const char* nombres[]={"Salud infinita","Stamina infinita","Espíritu infinito"};
    for(size_t i=0;i<3;++i) {
        bool activo=vitalActivo(i);
        ImGui::BeginDisabled(!vitalDisponible(i));
        if(ImGui::Checkbox(nombres[i],&activo)) { activarVital(i,activo); c.vitales[i]=activo; preferenciasCambiaron(); }
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s",estadoVital(i));
    }
    ImGui::Separator();
    bool vuelo=vueloActivo();
    ImGui::BeginDisabled(!vueloDisponible());
    if(ImGui::Checkbox("Free flight durante el planeo",&vuelo)) { activarVuelo(vuelo); c.vuelo=vuelo; preferenciasCambiaron(); }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoVuelo());
    if(ImGui::SliderFloat("Multiplicador hacia adelante",&c.vueloAdelante,1,5,"%.1f")) preferenciasCambiaron();
    if(ImGui::SliderFloat("Velocidad vertical solicitada",&c.vueloVertical,1,50,"%.1f")) preferenciasCambiaron();
    ImGui::TextWrapped("Controles: mantener %s o el botón del control asignado. Subir: %s; bajar: %s. En control: gatillos para subir/bajar y stick izquierdo hacia adelante. Solo actúa con planeo verificado.",
        nombreTecla(c.teclaVuelo).c_str(),nombreTecla(c.teclaSubir).c_str(),nombreTecla(c.teclaBajar).c_str());
}
void mundo() {
    auto& c=configuracion();
    bool activo=tiempoActivo();
    ImGui::BeginDisabled(!tiempoDisponible());
    if(ImGui::Checkbox("Control del tiempo",&activo)) { c.tiempo=activo; activarTiempo(activo); congelarTiempo(activo && c.congelado); preferenciasCambiaron(); }
    ImGui::BeginDisabled(!activo);
    if(ImGui::Checkbox("Congelar reloj y sol",&c.congelado)) { congelarTiempo(c.congelado); preferenciasCambiaron(); }
    static int hora=12,minuto=0;
    ImGui::SliderInt("Hora",&hora,0,23); ImGui::SliderInt("Minuto",&minuto,0,59);
    if(ImGui::Button("Aplicar hora")) { fijarHora(hora,minuto); avisoAccion="Solicitud enviada; espera a que avance el hilo del juego."; }
    ImGui::EndDisabled(); ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoTiempo());
    ImGui::TextWrapped("Cambiar la hora puede afectar horarios y misiones. Apagar el control permite que avance; no revierte el tiempo transcurrido.");
    ImGui::Separator();
    bool clima=climaActivo();
    ImGui::BeginDisabled(!climaDisponible() && !clima);
    if(ImGui::Checkbox("Control del clima",&clima)) { c.clima=clima; activarClima(clima); preferenciasCambiaron(); }
    ImGui::BeginDisabled(!clima);
    bool cambio=false;
    cambio|=ImGui::SliderFloat("Precipitación",&c.ajustesDelClima.precipitacion,0,1,"%.2f");
    cambio|=ImGui::SliderFloat("Nubosidad",&c.ajustesDelClima.nubosidad,-1,1,"%.2f");
    cambio|=ImGui::SliderFloat("Humedad",&c.ajustesDelClima.humedad,0,1,"%.2f");
    cambio|=ImGui::SliderFloat("Viento",&c.ajustesDelClima.viento,0,60,"%.2f");
    cambio|=ImGui::SliderFloat("Niebla atmosférica",&c.ajustesDelClima.niebla,0,10,"%.2f");
    cambio|=ImGui::Checkbox("Precipitación como nieve",&c.ajustesDelClima.nieve);
    if(cambio) { fijarClima(c.ajustesDelClima); preferenciasCambiaron(); }
    ImGui::EndDisabled(); ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoClima());
    ImGui::TextWrapped("La niebla atmosférica es un efecto del clima. El revelado del mapa está en su propia pestaña.");
}
void monturasYDelitos() {
    auto& c=configuracion();
    bool monturas=monturasNivel5Activas();
    ImGui::BeginDisabled(!monturasNivel5Disponible());
    if(ImGui::Checkbox("Velocidad de monturas: nivel 5 global",&monturas)) {
        c.monturasNivel5=monturas; activarMonturasNivel5(monturas); preferenciasCambiaron();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoMonturasNivel5());
    ImGui::TextWrapped("Alternativa: aplica el atributo de velocidad de nivel 5. No ofrece multiplicadores por especie. Apagar detiene futuras aplicaciones; no reduce valores ya cambiados y podrían guardarse.");
    ImGui::Separator();
    bool delitos=delitosActivos();
    ImGui::BeginDisabled(!delitosDisponible() || !motorDisponible());
    if(ImGui::Checkbox("Sin recompensa y exenciones parciales de delitos",&delitos)) {
        c.delitos=delitos; activarDelitos(delitos); preferenciasCambiaron();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoDelitos());
    ImGui::TextWrapped("No registrar todos los delitos: no disponible; falta un contrato universal verificado. Las reglas publicadas solo reducen recompensas y eximen categorías específicas; los testigos y las misiones pueden seguir reaccionando.");
}
void equipo() {
    auto& c=configuracion();
    ImGui::TextWrapped("Ediciones persistentes: apagar una opción cancela la cola; no deshace objetos ya editados. Haz respaldo antes de aplicar.");
    bool refinar=c.refinar && equipoActivo();
    ImGui::BeginDisabled(!refinarDisponible() || !motorDisponible());
    if(ImGui::Checkbox("Habilitar refinamiento máximo",&refinar)) {
        c.refinar=refinar; cancelarSolicitudesEquipo(); activarEquipo(c.refinar); preferenciasCambiaron();
    }
    ImGui::BeginDisabled(!refinar);
    if(ImGui::Button("Refinar armas y armadura compatibles")) ImGui::OpenPopup("Confirmar refinamiento");
    if(ImGui::BeginPopupModal("Confirmar refinamiento",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Se modificará el equipo compatible de la mochila y el que llevas puesto. El límite se lee por tipo, hasta nivel 10. Verifica atributos y persistencia en una copia de partida.");
        if(ImGui::Button("Aplicar refinamiento")) { avisoAccion=solicitarRefinarTodo() ? "Refinamiento en cola." : "No se pudo encolar."; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if(ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndDisabled(); ImGui::EndDisabled();
    ImGui::TextWrapped("Desbloquear todas las ranuras del equipo existente: parcial. Falta el contrato de persistencia; no se representa con offsets de una estructura supuesta.");
    ImGui::TextWrapped("%s",estadoEquipo());
    ImGui::Separator();
    bool nuevo=equipoNativoActivo();
    ImGui::BeginDisabled(!equipoNativoDisponible());
    if(ImGui::Checkbox("Alternativa: crear una pieza nueva con cinco ranuras",&nuevo)) {
        c.equipoNuevo=nuevo; activarEquipoNativo(nuevo); preferenciasCambiaron();
    }
    ImGui::BeginDisabled(!nuevo);
    static std::string filtroEquipo;
    static uint32_t seleccionado=0;
    ImGui::InputText("Filtrar catálogo de claves reales",&filtroEquipo);
    auto catalogo=catalogoEquipoNativo();
    ImGui::BeginChild("EquiposNativos",ImVec2(0,140),true);
    for(const auto& tipo:catalogo) {
        if(!filtroEquipo.empty() && tipo.nombre.find(filtroEquipo)==std::string::npos) continue;
        ImGui::PushID(std::to_string(tipo.claveExterna).c_str());
        if(ImGui::Selectable(tipo.nombre.c_str(),seleccionado==tipo.claveExterna)) seleccionado=tipo.claveExterna;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::TextWrapped("La fuente publica claves, no nombres localizados. Consulta el catálogo antes de crear una pieza. No se modifica la que llevas puesta.");
    ImGui::BeginDisabled(!seleccionado);
    if(ImGui::Button("Crear una pieza con cinco ranuras")) ImGui::OpenPopup("Confirmar equipo nuevo");
    if(ImGui::BeginPopupModal("Confirmar equipo nuevo",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Se creará UNA pieza nueva con clave real %u y cinco ranuras. Conserva su nivel original. Puede quedar en tu partida guardada.",seleccionado);
        if(ImGui::Button("Crear pieza")) { avisoAccion=solicitarEquipoNativo(seleccionado) ? "Creación nativa en cola." : "Creación rechazada; revisa el estado."; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if(ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndDisabled(); ImGui::EndDisabled(); ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoEquipoNativo());
}

void mapa() {
    auto& c=configuracion();
    const auto cantidad=regionCantidad();
    if(cantidad) {
        c.regionMapa=std::clamp(c.regionMapa,0,static_cast<int>(cantidad)-1);
        if(ImGui::BeginCombo("Región solicitada",nombreRegion(static_cast<size_t>(c.regionMapa)))) {
            for(size_t i=0;i<cantidad;++i) if(ImGui::Selectable(nombreRegion(i),static_cast<size_t>(c.regionMapa)==i)) {
                c.regionMapa=static_cast<int>(i); preferenciasCambiaron();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::TextWrapped("Revelar toda la niebla por región: parcial, mecánica sin verificar.");
    ImGui::TextWrapped("Desbloquear todos los nexos por región: parcial. Descubrimiento, estado de puerta y acertijos son capas distintas; falta verificar la operación completa.");
    ImGui::Separator();
    ImGui::TextWrapped("Alternativa: aprender descubrimientos individuales. La selección de región superior no filtra ni aplica este editor; no hay una correspondencia regional verificada.");
    bool activo=conocimientosActivos();
    ImGui::BeginDisabled(!descubrimientosDisponible() || !motorDisponible());
    if(ImGui::Checkbox("Habilitar editor de descubrimientos",&activo)) {
        c.conocimientos=activo; activarConocimientos(activo); preferenciasCambiaron();
    }
    ImGui::BeginDisabled(!activo);
    static std::string filtro;
    static uint32_t seleccion=0;
    ImGui::InputText("Buscar nombre interno",&filtro);
    auto entradas=leerDescubrimientos();
    ImGui::BeginChild("Descubrimientos",ImVec2(0,190),true);
    for(const auto& entrada:entradas) {
        if(!filtro.empty() && entrada.nombreInterno.find(filtro)==std::string::npos) continue;
        ImGui::PushID(std::to_string(entrada.clave).c_str());
        const auto nombre=entrada.nombreInterno+(entrada.aprendido ? " | aprendido" : " | pendiente");
        if(ImGui::Selectable(nombre.c_str(),seleccion==entrada.clave)) seleccion=entrada.clave;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(!seleccion);
    if(ImGui::Button("Aprender descubrimiento seleccionado")) avisoAccion=solicitarAprenderClave(seleccion) ? "Conocimiento en cola; espera la confirmación del juego." : "Solicitud rechazada.";
    ImGui::EndDisabled(); ImGui::EndDisabled(); ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoRegiones());
    ImGui::TextWrapped("Aprender puede conservarse en la partida. Apagar cancela pendientes y no borra conocimientos. No garantiza quitar niebla completa, resolver acertijos ni habilitar viajes.");
}

void inventario() {
    bool activo=inventarioActivo();
    ImGui::BeginDisabled(!inventarioDisponible() || !motorDisponible());
    if(ImGui::Checkbox("Habilitar editor de inventario",&activo)) {
        activarInventario(activo); configuracion().inventario=activo; preferenciasCambiaron();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",estadoInventario());
    ImGui::TextWrapped("Las ediciones pueden guardarse. Desactivar el editor cancela solicitudes pendientes; los objetos ya cambiados se conservan. Haz respaldo primero.");
    ImGui::BeginDisabled(!activo);
    static std::string filtro;
    ImGui::InputText("Buscar objeto que llevas",&filtro);
    static uint64_t seleccion=0;
    static int64_t cantidad=1;
    auto objetos=leerInventario();
    ImGui::BeginChild("Objetos",ImVec2(0,190),true);
    for(const auto& item:objetos) {
        if(!filtro.empty() && item.nombre.find(filtro)==std::string::npos) continue;
        ImGui::PushID(std::to_string(item.idInstancia).c_str());
        const auto etiqueta=item.nombre+" | "+item.almacen+" | "+std::to_string(item.cantidad);
        if(ImGui::Selectable(etiqueta.c_str(),seleccion==item.idInstancia)) { seleccion=item.idInstancia; cantidad=item.cantidad; }
        ImGui::PopID();
    }
    ImGui::EndChild();
    const auto encontrado=std::find_if(objetos.begin(),objetos.end(),[](const auto& item){return item.idInstancia==seleccion;});
    ImGui::BeginDisabled(encontrado==objetos.end() || !encontrado->editable);
    ImGui::InputScalar("Nueva cantidad",ImGuiDataType_S64,&cantidad);
    if(ImGui::Button("Cambiar cantidad")) avisoAccion=solicitarCantidad(seleccion,cantidad) ? "Cambio en cola; revisa el estado del editor." : "Solicitud rechazada; revisa cantidad, objeto y estado.";
    ImGui::SameLine();
    if(ImGui::Button("Eliminar objeto")) ImGui::OpenPopup("Confirmar eliminación");
    if(ImGui::BeginPopupModal("Confirmar eliminación",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Se eliminará el objeto seleccionado. Puede guardarse en tu partida.");
        if(ImGui::Button("Eliminar")) { avisoAccion=solicitarEliminar(seleccion) ? "Eliminación en cola." : "No se pudo encolar la eliminación."; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if(ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    // Los tipos y nombres proceden del catálogo real; no se presupone una tabla estática.
    static std::string filtroCatalogo;
    static int tipo=0;
    static int64_t cuantos=1;
    ImGui::InputText("Buscar en catálogo para añadir",&filtroCatalogo);
    auto catalogo=leerCatalogoInventario();
    ImGui::BeginChild("Catálogo",ImVec2(0,140),true);
    for(const auto& item:catalogo) {
        if(!filtroCatalogo.empty() && item.nombre.find(filtroCatalogo)==std::string::npos) continue;
        ImGui::PushID(static_cast<int>(item.tipoId));
        if(ImGui::Selectable((item.nombre+" | "+item.categoria).c_str(),tipo==item.tipoId)) {
            tipo=item.tipoId; cuantos=1;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if(catalogo.empty()) ImGui::TextWrapped("Esperando catálogo del juego. Abre su inventario para que el motor consulte sus tablas.");
    ImGui::Text("ID seleccionado: %d",tipo);
    ImGui::InputScalar("Cantidad a añadir",ImGuiDataType_S64,&cuantos);
    ImGui::BeginDisabled(!agregarInventarioDisponible());
    if(ImGui::Button("Añadir objeto seleccionado")) avisoAccion=(tipo>0 && tipo<65535 && solicitarAgregar(static_cast<uint16_t>(tipo),cuantos)) ? "Alta en cola; revisa el resultado del editor." : "Alta rechazada: selección, cantidad o estado no válidos.";
    ImGui::EndDisabled();
    if(!agregarInventarioDisponible()) ImGui::TextWrapped("Añadir no disponible: faltan datos de construcción, confirmación o destrucción de objetos.");
    ImGui::TextWrapped("El motor valida el ID contra su tabla real; no se incluyen IDs supuestos.");
    ImGui::EndDisabled();
}
void ajustes() {
    auto& c=configuracion();
    filaTecla("Abrir/cerrar menú",1,c.teclaMenu); filaTecla("Activar vuelo al mantener",2,c.teclaVuelo);
    filaTecla("Ascender en vuelo",3,c.teclaSubir); filaTecla("Descender en vuelo",4,c.teclaBajar);
    if(capturar) ImGui::TextUnformatted("Presiona una tecla. Esc cancela la reasignación.");
    constexpr const char* botones[]={"A","B","X","Y","LB","RB","Clic stick izquierdo","Clic stick derecho"};
    constexpr int valores[]={0x1000,0x2000,0x4000,0x8000,0x100,0x200,0x40,0x80};
    int elegido=7; for(int i=0;i<8;++i) if(c.botonVuelo==valores[i]) elegido=i;
    if(ImGui::Combo("Botón de vuelo (XInput)",&elegido,botones,8)) { c.botonVuelo=valores[elegido]; preferenciasCambiaron(); }
    ImGui::TextWrapped("Control Xbox o compatible con XInput, primer control conectado. Para un control PlayStation usa la conversión de Steam Input a XInput.");
    if(ImGui::Button("Guardar preferencias")) guardarConfiguracion();
    ImGui::SameLine(); if(ImGui::Button("Restablecer tecla INICIO")) { c.teclaMenu=VK_HOME; preferenciasCambiaron(); }
    ImGui::TextWrapped("%s",estadoGuardado());
    ImGui::TextUnformatted("Archivo: %LOCALAPPDATA%\\DesiertoSeguro\\DesiertoSeguro.ini");
}
}
bool menuVisible() { return abierto.load(); }
void dibujarMenu() {
    // Recupera opciones solo cuando el renderer ya logró dibujar; jamás a ciegas durante la carga.
    static bool recuperadas=false;
    if(!recuperadas && listo.load(std::memory_order_acquire)) { aplicarPreferencias(); recuperadas=true; }
    actualizarClima();
    entradas();
    if(!abierto.load()) { ImGui::GetIO().MouseDrawCursor=false; return; }
    ImGui::GetIO().MouseDrawCursor=true;
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::SetNextWindowSize(ImVec2(830,690),ImGuiCond_FirstUseEver);
    bool mostrar=true;
    if(ImGui::Begin("DesiertoSeguro | datos públicos y estado real",&mostrar)) {
        ImGui::TextWrapped("Proyecto experimental: datos publicados de otros parches. No hay prueba en tu ejecutable. Usa una copia de la partida y activa una función a la vez.");
        ImGui::TextWrapped("%s",estadoMotor());
        ImGui::Separator();
        if(falloInicio.load(std::memory_order_acquire)) ImGui::TextWrapped("La inicialización terminó con un error. No se aplicaron las preferencias guardadas. Revisa DesiertoSeguro.log y reinicia el juego.");
        else if(!listo.load(std::memory_order_acquire)) ImGui::TextUnformatted("Buscando y validando datos del juego...");
        else if(ImGui::BeginTabBar("Funciones")) {
            if(ImGui::BeginTabItem("Personaje y vuelo")) { jugador(); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Tiempo y clima")) { mundo(); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Monturas y delitos")) { monturasYDelitos(); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Equipo")) { equipo(); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Inventario")) { inventario(); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Mapa por región")) { mapa(); ImGui::EndTabItem(); }
            if(ImGui::BeginTabItem("Configuración")) { ajustes(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        if(!avisoAccion.empty()) { ImGui::Separator(); ImGui::TextWrapped("%s",avisoAccion.c_str()); }
    }
    ImGui::End();
    if(!mostrar) abierto=false;
}
}
