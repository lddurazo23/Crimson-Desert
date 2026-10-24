#include "inventario.hpp"
#include "equipo.hpp"
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include "inventario_puente.hpp"

#include "inventario_motor.inc"

namespace ds {
uintptr_t resolverTablaPublica(const char* nombre,bool indirecta) {
    return trinity::game::FindTableGlobal(nombre,indirecta);
}
namespace mem = trinity::mem;
namespace {
std::atomic_bool instalado{false}, activo{false}, bloqueado{false}, disenoVerificado{false};
std::atomic_flag enProceso = ATOMIC_FLAG_INIT;
std::mutex proteccion;
std::vector<ItemInventario> copia;
std::vector<TipoInventario> catalogoCopia;
std::string estado = "No disponible: faltan datos o no se ha validado esta versión.";
enum class Accion { Cantidad, Agregar, Eliminar };
struct Solicitud { Accion accion; uint64_t id; uint16_t tipo; int64_t cantidad; };
std::optional<Solicitud> pendiente;
void informar(const char* mensaje) { std::lock_guard<std::mutex> lk(proteccion); estado = mensaje; }
int64_t limitePara(uint16_t tipo) {
    using namespace trinity::game;
    uintptr_t def = 0;
    if (!DefForRow(g_itemTableGlobal, tipo, &def)) return 0;
    int64_t maximo = 0; uint8_t aplica = 0;
    if (!mem::Read64(def + kOff_ItemDef_MaxStackCount, &maximo) ||
        !mem::Read8(def + kOff_ItemDef_ApplyMaxStackCap, &aplica) || aplica > 1) return 0;
    if (aplica && maximo <= 0) return 0;
    return aplica ? std::min<int64_t>(100000, maximo) : 100000;
}
bool clasificarEquipo(uint16_t tipo,bool* refinable,bool* ranuras,uint16_t* maximo) {
    using namespace trinity::game;
    *refinable=false; *ranuras=false; *maximo=0;
    char clave[256]{};
    if(!KeyForType(tipo,clave,sizeof(clave))) return false;
    const std::string_view nombre(clave);
    const bool arma=nombre.starts_with("Equip_Weapon");
    const bool armadura=nombre.starts_with("Equip_Armor");
    if(!arma && !armadura) return false;
    uintptr_t def=0,curva=0; uint32_t cantidad=0;
    if(!DefForRow(g_itemTableGlobal,tipo,&def) ||
       !leer(def+datos::equipo::listaRefinamientoTipo,curva) || curva<kMinPointer ||
       !leer(def+datos::equipo::cantidadRefinamientoTipo,cantidad) || cantidad<2 || cantidad>datos::equipo::cantidadRefinamientosLimite) return false;
    uint8_t testigo=0;
    if(!leer(curva,testigo)) return false;
    *maximo=static_cast<uint16_t>(std::min<uint32_t>(datos::equipo::refinamientoMaximo,cantidad-1));
    *refinable=true; *ranuras=true; return true;
}
bool escribirComprobado(uintptr_t direccion, const void* valor, size_t n) {
    if (!escribirDatos(direccion, valor, n)) return false;
    uint8_t comprobar[8]{};
    return n <= sizeof(comprobar) && leer(direccion, comprobar, n) && std::memcmp(comprobar, valor, n) == 0;
}
// Una operación entre los dos mirrors no es una transacción atómica del juego.
// Si falla una escritura se intenta restaurar y se bloquean las ediciones de la sesión.
bool modificar(const Solicitud& solicitud) {
    using namespace trinity::game;
    const auto cliente = CurrentHolder();
    const auto autoridad = ServerHolder();
    if (!cliente || !autoridad || cliente == autoridad) return false;
    Item* encontrado = nullptr;
    for (auto& almacen : g_storages) for (auto& grupo : almacen.groups) for (auto& item : grupo.items) {
        uint64_t id = 0;
        if (!mem::Read64(item.slot + kOff_ItemVal_InstanceId, &id)) continue;
        if (id == solicitud.id && item.typeId == solicitud.tipo) {
            if (encontrado) return false;
            encontrado = &item;
        }
    }
    if (!encontrado) return false;
    const auto servidor = SlotByPos(autoridad, encontrado->bucketIdx, encontrado->slotIdx, solicitud.tipo);
    const auto clienteSlot = SlotByPos(cliente, encontrado->bucketIdx, encontrado->slotIdx, solicitud.tipo);
    if (!servidor || !clienteSlot || clienteSlot != encontrado->slot || servidor == clienteSlot) return false;
    uint64_t idCliente = 0, idServidor = 0;
    int64_t cantidadCliente = 0, cantidadServidor = 0;
    if (!mem::Read64(clienteSlot + kOff_ItemVal_InstanceId, &idCliente) ||
        !mem::Read64(servidor + kOff_ItemVal_InstanceId, &idServidor) ||
        idCliente != solicitud.id || idServidor != solicitud.id ||
        !mem::Read64(clienteSlot + kOff_InvSlot_Quantity, &cantidadCliente) ||
        !mem::Read64(servidor + kOff_InvSlot_Quantity, &cantidadServidor) ||
        cantidadCliente != cantidadServidor) return false;
    const auto limite = limitePara(solicitud.tipo);
    const auto nueva = solicitud.accion == Accion::Eliminar ? int64_t(0) : solicitud.cantidad;
    if (nueva < 0 || limite <= 0 || nueva > limite) return false;
    bool ok = escribirComprobado(servidor + kOff_InvSlot_Quantity, &nueva, sizeof(nueva)) &&
              escribirComprobado(clienteSlot + kOff_InvSlot_Quantity, &nueva, sizeof(nueva));
    const uint16_t vacio = kInvSlot_EmptyType;
    if (ok && nueva == 0) {
        ok = escribirComprobado(servidor + kOff_InvSlot_TypeId, &vacio, sizeof(vacio)) &&
             escribirComprobado(clienteSlot + kOff_InvSlot_TypeId, &vacio, sizeof(vacio));
    }
    if (!ok) {
        bool restaurado = escribirComprobado(servidor + kOff_InvSlot_Quantity, &cantidadServidor, sizeof(cantidadServidor));
        restaurado = escribirComprobado(clienteSlot + kOff_InvSlot_Quantity, &cantidadCliente, sizeof(cantidadCliente)) && restaurado;
        restaurado = escribirComprobado(servidor + kOff_InvSlot_TypeId, &solicitud.tipo, sizeof(solicitud.tipo)) && restaurado;
        restaurado = escribirComprobado(clienteSlot + kOff_InvSlot_TypeId, &solicitud.tipo, sizeof(solicitud.tipo)) && restaurado;
        (void)restaurado;
        falloParcialInventario();
        return false;
    }
    const bool autoridadReparada=RepairUsedSlots(autoridad);
    const bool clienteReparado=RepairUsedSlots(cliente);
    if(!autoridadReparada || !clienteReparado) { falloParcialInventario(); return false; }
    return true;
}
bool agregarProtegido(uint16_t tipo, int64_t cantidad) {
    __try { return trinity::game::CommitAdd(tipo, cantidad); }
    __except(EXCEPTION_EXECUTE_HANDLER) { ds::falloParcialInventario(); return false; }
}
void actualizarCopia() {
    using namespace trinity::game;
    std::vector<ItemInventario> filas;
    for (const auto& almacen : g_storages) for (const auto& grupo : almacen.groups) for (const auto& item : grupo.items) {
        uint64_t id = 0;
        const auto maximo = limitePara(item.typeId);
        const bool idValido = mem::Read64(item.slot + kOff_ItemVal_InstanceId, &id) && id > 0 && id != UINT64_MAX;
        filas.push_back({id, item.typeId, item.name, almacen.name, grupo.label, item.qty, maximo, idValido && maximo > 0});
    }
    std::unordered_map<uint64_t, size_t> cuentas;
    for (const auto& fila : filas) ++cuentas[fila.idInstancia];
    for (auto& fila : filas) if (cuentas[fila.idInstancia] != 1) fila.editable = false;
    std::lock_guard<std::mutex> lk(proteccion);
    copia = std::move(filas);
}
void actualizarCatalogo() {
    using namespace trinity::game;
    if (!catalogoCopia.empty()) return;
    if (!Inventory::CatalogReady()) return;
    std::vector<TipoInventario> tipos;
    for (const auto& grupo : g_catalog) for (const auto& item : grupo.items) {
        const auto limite = limitePara(item.typeId);
        if (limite > 0) tipos.push_back({item.typeId, item.name, grupo.label, limite});
    }
    std::lock_guard<std::mutex> lk(proteccion);
    catalogoCopia = std::move(tipos);
}
bool encolar(Accion accion, uint64_t id, uint16_t tipo, int64_t cantidad) {
    if (!instalado.load() || !activo.load() || bloqueado.load() ||
        (accion==Accion::Agregar && !agregarInventarioDisponible())) return false;
    std::lock_guard<std::mutex> lk(proteccion);
    if (pendiente) return false;
    if (accion != Accion::Agregar) {
        size_t encontrados = 0;
        for (const auto& item : copia) if (item.idInstancia == id && item.editable) {
            tipo = item.tipoId; ++encontrados;
            if (cantidad < 0 || cantidad > item.cantidadMaxima) return false;
        }
        if (encontrados != 1) return false;
    } else if (tipo == trinity::game::kInvSlot_EmptyType || cantidad < 1 || cantidad > 100000) return false;
    pendiente = Solicitud{accion, id, tipo, cantidad};
    estado = "Solicitud pendiente; esperando un tick seguro del motor.";
    return true;
}
}
bool iniciarInventario() {
    if (instalado.load()) return true;
    // Un prólogo conservado entre parches no demuestra el tamaño que escribe el constructor.
    // Los dos testigos deben pertenecer a las mismas funciones del perfil C0 publicado.
    const auto ctor=buscarUnico(trinity::game::kSig_TrItemValueCtor);
    const auto consulta=buscarUnico(trinity::game::kSig_InvGetItemQty);
    if(!testigoEnFuncion(ctor,datos::inventarioLegado::firmaTestigoCtorC0) ||
       !testigoEnFuncion(consulta,datos::inventarioLegado::firmaTestigoPasoC0)) {
        informar("no disponible: faltan datos que acrediten el diseño C0 del inventario; el perfil C8 requiere otra ruta nativa.");
        return false;
    }
    disenoVerificado.store(true);
    const auto mh = MH_Initialize();
    if (mh != MH_OK && mh != MH_ERROR_ALREADY_INITIALIZED) {
        informar("No disponible: MinHook no pudo inicializarse."); return false;
    }
    const bool montado = trinity::game::Inventory::Install();
    const bool ok = montado && trinity::game::g_coreGlobal && trinity::game::g_commitTarget &&
                    trinity::game::g_itemTableGlobal && trinity::game::oNtQueryInfoThread;
    instalado.store(ok);
    informar(ok ? "Adaptador listo; activa el editor después de cargar una copia de tu partida." :
                  "No disponible: faltan datos o alguna firma no tiene una coincidencia única.");
    return ok;
}
bool disenoInventarioLegadoVerificado() {return disenoVerificado.load();}
bool inventarioDisponible() { return instalado.load() && !bloqueado.load(); }
bool agregarInventarioDisponible() {
    using namespace trinity::game;
    return inventarioDisponible() && oItemValueCtor && oHolderInsert &&
        oCommitPlacement && oFreePlacements && oItemValueDtor && oNtQueryInfoThread;
}
bool inventarioActivo() { return activo.load() && inventarioDisponible(); }
const char* estadoInventario() {
    thread_local std::string local;
    std::lock_guard<std::mutex> lk(proteccion); local = estado;
    return local.c_str();
}
bool activarInventario(bool valor) {
    if (valor && !inventarioDisponible()) return false;
    activo.store(valor);
    if (!valor) {
        std::lock_guard<std::mutex> lk(proteccion);
        pendiente.reset(); copia.clear();
        if (!bloqueado.load()) estado = "Editor desactivado; las operaciones ya guardadas no se revierten.";
    }
    return true;
}
std::vector<ItemInventario> leerInventario() {
    std::lock_guard<std::mutex> lk(proteccion); return copia;
}
std::vector<TipoInventario> leerCatalogoInventario() {
    std::lock_guard<std::mutex> lk(proteccion); return catalogoCopia;
}
bool solicitarCantidad(uint64_t id, int64_t cantidad) { return encolar(Accion::Cantidad, id, 0, cantidad); }
bool solicitarEliminar(uint64_t id) { return encolar(Accion::Eliminar, id, 0, 0); }
bool solicitarAgregar(uint16_t tipo, int64_t cantidad) { return encolar(Accion::Agregar, 0, tipo, cantidad); }
void falloParcialInventario() {
    bloqueado.store(true); activo.store(false);
    informar("Ediciones bloqueadas: operación parcial o excepción. Reinicia el juego y restaura el respaldo antes de continuar.");
}
void procesarInventarioInterno() {
    if ((!inventarioActivo() && !equipoActivo()) || enProceso.test_and_set(std::memory_order_acquire)) return;
    struct Liberar { ~Liberar() { enProceso.clear(std::memory_order_release); } } liberar;
    using namespace trinity::game;
    // El realm se verifica en el hilo que ejecutará las llamadas, sin reutilizar TLS de otro hilo.
    uint8_t realm = 0xFF;
    if (!RealmFlagAddr(&realm) || realm > 1) return;
    const auto cliente = CurrentHolder();
    const auto autoridad = ServerHolder();
    if (!cliente || !autoridad || cliente == autoridad) {
        informar("Esperando los dos mirrors válidos del inventario; abre el inventario del juego o recarga la partida.");
        return;
    }
    std::optional<Solicitud> solicitud;
    {
        std::lock_guard<std::mutex> lk(proteccion);
        solicitud = pendiente; pendiente.reset();
    }
    static ULONGLONG ultimaCopia = 0;
    const auto ahora = GetTickCount64();
    if (solicitud || ahora - ultimaCopia >= 250) {
        Inventory::ForceRefresh();
        if(inventarioActivo()) actualizarCatalogo();
        ultimaCopia = ahora;
    }
    if (solicitud && inventarioActivo()) {
        bool ok = false;
        if (solicitud->accion == Accion::Agregar) {
            const auto limite = limitePara(solicitud->tipo);
            const bool rutaCompleta = oItemValueCtor && oHolderInsert && oCommitPlacement && oFreePlacements && oItemValueDtor;
            if (rutaCompleta && limite > 0 && solicitud->cantidad <= limite)
                ok = agregarProtegido(solicitud->tipo, solicitud->cantidad);
        } else ok = modificar(*solicitud);
        if (!bloqueado.load()) informar(ok ? "Operación terminada en ambos mirrors. Verifica el objeto en el juego antes de guardar." :
                                            "Operación rechazada: cambió el objeto, falta un dato o el motor no la aceptó.");
        Inventory::ForceRefresh();
    }
    if(equipoActivo()) {
        std::vector<ParEquipo> pares;
        for(const auto& almacen:g_storages) if(std::strcmp(almacen.key,"Character")==0) for(const auto& grupo:almacen.groups) for(const auto& item:grupo.items) {
            bool refina=false,ranuras=false; uint16_t maximo=0;
            if(!clasificarEquipo(item.typeId,&refina,&ranuras,&maximo)) continue;
            uint64_t id=0;
            const auto ladoA=SlotByPos(cliente,item.bucketIdx,item.slotIdx,item.typeId);
            const auto ladoB=SlotByPos(autoridad,item.bucketIdx,item.slotIdx,item.typeId);
            if(!ladoA || ladoA!=item.slot || !ladoB || ladoA==ladoB ||
               !mem::Read64(ladoA+kOff_ItemVal_InstanceId,&id) || !id || id==UINT64_MAX) continue;
            pares.push_back({ladoA,ladoB,id,item.typeId,refina,ranuras,maximo});
        }
        procesarEquipo(pares.data(),pares.size(),clasificarEquipo);
    }
    if (inventarioActivo()) actualizarCopia();
}
void procesarInventario() {
    try { procesarInventarioInterno(); }
    catch(...) { falloParcialInventario(); }
}
}
