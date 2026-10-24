#include "teclas.hpp"
#include <cassert>
#include <iostream>
int main() {
    for(int menu=8;menu<=254;++menu) {
        const auto t=ds::normalizarTeclas({menu,menu,menu,menu});
        assert(t[0]==menu);
        for(size_t i=0;i<t.size();++i) for(size_t j=0;j<i;++j) assert(t[i]!=t[j]);
    }
    const auto original=std::array<int,4>{0x24,0x75,0x20,0x11};
    assert(ds::normalizarTeclas(original)==original);
    const auto fallos=ds::normalizarTeclas({-1,300,0,0});
    assert(fallos==original);
    std::cout<<"Teclas: preferencias válidas y colisiones al cargar comprobadas.\n";
}
