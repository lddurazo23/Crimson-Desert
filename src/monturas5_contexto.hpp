#pragma once
#include <cstddef>
#include <cstdint>

// Marco propio del gateway. Estos offsets describen NUESTRA pila, no el juego.
struct ContextoMonturas5 {
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rbx,rdx,rcx,rax,banderas;
};
static_assert(sizeof(ContextoMonturas5)==128);
static_assert(offsetof(ContextoMonturas5,r12)==24);
static_assert(offsetof(ContextoMonturas5,r14)==8);
static_assert(offsetof(ContextoMonturas5,rdx)==96);

namespace ds::monturas5 {
struct CampoNivel {
    uint64_t actual=0,maximo=0,minimo=0,paso=0;
    uint8_t estadistica=0,subtipo=0;
    uint16_t ordinal=0;
};
}
