#pragma once
#include <array>
#include <cstddef>
#include <algorithm>

namespace ds {
// Virtual-Key públicos de Windows; no son datos internos del juego.
inline std::array<int,4> normalizarTeclas(std::array<int,4> teclas) {
    constexpr std::array<int,4> predeterminadas{0x24,0x75,0x20,0x11};
    for(size_t i=0;i<teclas.size();++i) {
        const auto libre=[&](int valor) {
            return valor>=8 && valor<=254 &&
                std::find(teclas.begin(),teclas.begin()+static_cast<ptrdiff_t>(i),valor)==teclas.begin()+static_cast<ptrdiff_t>(i);
        };
        if(libre(teclas[i])) continue;
        teclas[i]=predeterminadas[i];
        if(libre(teclas[i])) continue;
        for(int candidato:predeterminadas) if(libre(candidato)) {teclas[i]=candidato;break;}
        if(!libre(teclas[i])) for(int candidato=0x70;candidato<=0x87;++candidato)
            if(libre(candidato)) {teclas[i]=candidato;break;}
    }
    return teclas;
}
}
