#pragma once
#include <cpuid.h>
inline long long _InterlockedCompareExchange64(volatile long long* p,long long nuevo,long long previo) {
    return __sync_val_compare_and_swap(p,previo,nuevo);
}
