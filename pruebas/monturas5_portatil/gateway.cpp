#include "monturas5_contexto.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cpuid.h>
#include <cstring>
#include <iostream>
extern "C" {
void DsMonturasGateway();
uint64_t DsMonturasLeerXcr0();
void ProbarGateway();
void CapturarGateway();
void* DsMonturasTrampolin=reinterpret_cast<void*>(&CapturarGateway);
uint32_t DsMonturasXsaveBytes=0,DsMonturasXsaveLo=0,DsMonturasXsaveHi=0;
extern const uint32_t DsMonturasMxcsr=0x1F80;
alignas(64) uint64_t CapturaRegistros[16]{};
alignas(64) unsigned char CapturaVectores[16][32]{};
alignas(64) unsigned char VectorSemilla[32]{};
}
int main() {
    unsigned a=0,b=0,c=0,d=0;
    __cpuid(1,a,b,c,d);
    if((c&((1u<<26)|(1u<<27)|(1u<<28)))!=((1u<<26)|(1u<<27)|(1u<<28))) {
        std::cout<<"Gateway: CPU sin XSAVE/AVX; el mod rechaza el perfil de contexto no compatible.\n";return 0;
    }
    const uint64_t mask=DsMonturasLeerXcr0();
    if((mask&7)!=7) {std::cout<<"Gateway: AVX no habilitado por el sistema.\n";return 0;}
    __cpuid_count(0x0D,0,a,b,c,d);
    assert(b>=576 && b<=32768);
    DsMonturasXsaveBytes=b;DsMonturasXsaveLo=static_cast<uint32_t>(mask);DsMonturasXsaveHi=static_cast<uint32_t>(mask>>32);
    for(size_t i=0;i<32;++i) VectorSemilla[i]=static_cast<unsigned char>(i*7+3);
    for(unsigned ciclo=0;ciclo<200;++ciclo) {
        ProbarGateway();
        for(size_t i=0;i<15;++i) assert(CapturaRegistros[i]==(i==12 ? 5000u : 16u-i));
        assert(CapturaRegistros[15]&1u);
        assert((CapturaRegistros[15]&(1u<<10))==0);
        for(auto& vector:CapturaVectores) assert(std::memcmp(vector,VectorSemilla,32)==0);
    }
    std::cout<<"Gateway real: 200 ciclos preservan 15 GPR, banderas y los 16 YMM tras un callback que los destruye; sólo cambia RDX.\n";
}
