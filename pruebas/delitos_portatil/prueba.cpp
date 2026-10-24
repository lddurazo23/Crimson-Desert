#include "delitos.hpp"
#include "datos_juego.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using Bytes = std::array<unsigned char, 128>;
Bytes tablaWanted{}, tablaTribu{}, tablaNueva{}, wanted0{}, wanted1{}, wanted2{}, tribu0{}, tribu1{}, wantedNuevo{};
std::array<uintptr_t, 3> filasWanted{}, filasNuevas{};
std::array<uintptr_t, 2> filasTribu{};
uintptr_t globalWanted{}, globalTribu{}, denegar{};
bool personaje = true;
std::vector<std::pair<uintptr_t, size_t>> regiones;
template<class T> void poner(Bytes& b, size_t o, T v) { std::memcpy(b.data()+o, &v, sizeof(v)); }
template<class T> T sacar(const Bytes& b, size_t o) { T v{}; std::memcpy(&v, b.data()+o, sizeof(v)); return v; }
template<class T> void agregar(T& x) { regiones.push_back({reinterpret_cast<uintptr_t>(&x), sizeof(x)}); }
bool rango(uintptr_t p, size_t n) {
    for (auto [a, z] : regiones) if (p >= a && p-a <= z && n <= z-(p-a)) return true;
    return false;
}
namespace ds {
bool leer(uintptr_t p, void* salida, size_t n) {
    if (!rango(p,n)) return false;
    std::memcpy(salida, reinterpret_cast<const void*>(p), n); return true;
}
bool escribirDatos(uintptr_t p, const void* entrada, size_t n) {
    if (!rango(p,n) || p == denegar) return false;
    std::memcpy(reinterpret_cast<void*>(p), entrada, n); return true;
}
uintptr_t buscarUnico(const char*) { return 0; }
uintptr_t resolverRip(uintptr_t, uint32_t, uint32_t) { return 0; }
uintptr_t resolverTablaPublica(const char* nombre, bool) {
    if (std::strcmp(nombre, "WantedInfo") == 0) return reinterpret_cast<uintptr_t>(&globalWanted);
    if (std::strcmp(nombre, "TribeInfo") == 0) return reinterpret_cast<uintptr_t>(&globalTribu);
    return 0;
}
bool resolverPersonajeControlado(uintptr_t& propietario, uintptr_t& actor) {
    propietario = personaje ? 1 : 0; actor = personaje ? 2 : 0; return personaje;
}
}
void preparar() {
    for (auto* x : {&tablaWanted,&tablaTribu,&tablaNueva,&wanted0,&wanted1,&wanted2,&tribu0,&tribu1,&wantedNuevo}) agregar(*x);
    agregar(filasWanted); agregar(filasNuevas); agregar(filasTribu); agregar(globalWanted); agregar(globalTribu);
    globalWanted = reinterpret_cast<uintptr_t>(tablaWanted.data());
    globalTribu = reinterpret_cast<uintptr_t>(tablaTribu.data());
    filasWanted = {reinterpret_cast<uintptr_t>(wanted0.data()),reinterpret_cast<uintptr_t>(wanted1.data()),reinterpret_cast<uintptr_t>(wanted2.data())};
    filasTribu = {reinterpret_cast<uintptr_t>(tribu0.data()),reinterpret_cast<uintptr_t>(tribu1.data())};
    poner(tablaWanted,0x08,uint32_t(3)); poner(tablaWanted,0x58,reinterpret_cast<uintptr_t>(filasWanted.data()));
    poner(tablaTribu,0x08,uint32_t(2)); poner(tablaTribu,0x58,reinterpret_cast<uintptr_t>(filasTribu.data()));
    poner(wanted0,0x18,int64_t(100)); poner(wanted1,0x18,int64_t(200)); poner(wanted2,0x18,int64_t(300));
    poner(wanted2,0x10,uint8_t(1));
    poner(tribu0,0x50,uint8_t(2)); poner(tribu1,0x50,uint8_t(5));
}
void activar() { ds::activarDelitos(true); ds::actualizarDelitos(); assert(ds::delitosActivos()); }
void comprobarActivo() {
    assert(sacar<int64_t>(wanted0,0x18)==0); assert(sacar<int64_t>(wanted1,0x18)==0);
    assert(sacar<int64_t>(wanted2,0x18)==300); assert(sacar<uint8_t>(tribu0,0x50)==7); assert(sacar<uint8_t>(tribu1,0x50)==7);
}
void comprobarRestaurado() {
    assert(sacar<int64_t>(wanted0,0x18)==100); assert(sacar<int64_t>(wanted1,0x18)==200);
    assert(sacar<uint8_t>(tribu0,0x50)==2); assert(sacar<uint8_t>(tribu1,0x50)==5);
}
int main(int argc, char** argv) {
    assert(argc==2); const std::string caso=argv[1]; preparar();
    if (caso=="sentinel") { assert(!ds::iniciarDelitos()); assert(!ds::delitosDisponible()); comprobarRestaurado(); return 0; }
    assert(ds::iniciarDelitos()); assert(ds::delitosDisponible()); assert(!ds::delitosActivos());
    if (caso=="espera") {
        personaje=false; ds::activarDelitos(true); ds::actualizarDelitos(); assert(!ds::delitosActivos()); comprobarRestaurado();
        personaje=true; ds::actualizarDelitos(); assert(ds::delitosActivos()); comprobarActivo();
    } else if (caso=="rechazo") {
        denegar=reinterpret_cast<uintptr_t>(wanted1.data())+0x18; ds::activarDelitos(true); ds::actualizarDelitos();
        assert(!ds::delitosActivos()); assert(!ds::delitosDisponible()); comprobarRestaurado();
    } else if (caso=="externo") {
        activar(); comprobarActivo(); poner(wanted0,0x18,int64_t(888)); ds::actualizarDelitos();
        assert(!ds::delitosActivos()); assert(!ds::delitosDisponible());
        assert(sacar<int64_t>(wanted0,0x18)==888); assert(sacar<int64_t>(wanted1,0x18)==200); assert(sacar<uint8_t>(tribu0,0x50)==2);
    } else if (caso=="recarga") {
        activar(); filasNuevas[0]=reinterpret_cast<uintptr_t>(wantedNuevo.data());
        poner(wantedNuevo,0x18,int64_t(900)); poner(tablaNueva,0x08,uint32_t(1));
        poner(tablaNueva,0x58,reinterpret_cast<uintptr_t>(filasNuevas.data()));
        globalWanted=reinterpret_cast<uintptr_t>(tablaNueva.data()); ds::actualizarDelitos(); assert(ds::delitosActivos());
        assert(sacar<int64_t>(wantedNuevo,0x18)==0); ds::activarDelitos(false); ds::actualizarDelitos();
        assert(sacar<int64_t>(wantedNuevo,0x18)==900); assert(sacar<int64_t>(wanted0,0x18)==0);
        assert(!ds::delitosActivos()); assert(sacar<uint8_t>(tribu0,0x50)==2);
    } else if (caso=="fila_tardia") {
        filasWanted[1]=0; activar(); assert(sacar<int64_t>(wanted1,0x18)==200);
        filasWanted[1]=reinterpret_cast<uintptr_t>(wanted1.data()); ds::actualizarDelitos(); comprobarActivo();
        ds::activarDelitos(false); ds::actualizarDelitos(); comprobarRestaurado();
    } else if (caso=="reintento") {
        activar(); denegar=reinterpret_cast<uintptr_t>(wanted0.data())+0x18;
        ds::activarDelitos(false); ds::actualizarDelitos(); assert(ds::delitosActivos());
        assert(sacar<int64_t>(wanted0,0x18)==0); assert(sacar<int64_t>(wanted1,0x18)==200);
        denegar=0; ds::actualizarDelitos(); assert(!ds::delitosActivos()); comprobarRestaurado();
    } else if (caso=="restauracion") {
        activar(); comprobarActivo(); ds::actualizarDelitos(); comprobarActivo();
        ds::activarDelitos(false); assert(sacar<int64_t>(wanted0,0x18)==0);
        ds::actualizarDelitos(); assert(!ds::delitosActivos()); comprobarRestaurado();
        activar(); comprobarActivo(); ds::activarDelitos(false); ds::actualizarDelitos(); comprobarRestaurado();
    } else assert(false);
    std::cout << "Delitos: " << caso << " aprobado\n";
}
