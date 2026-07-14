#pragma once

#include <windows.h>

static inline long os_atomic_inc_long(volatile long* val) {
    return InterlockedIncrement(val);
}

static inline long os_atomic_dec_long(volatile long* val) {
    return InterlockedDecrement(val);
}

static inline long os_atomic_load_long(const volatile long* ptr) {
    return *ptr;
}

static inline bool os_atomic_set_bool(volatile bool* ptr, bool val) {
    const bool old = *ptr;
    *ptr = val;
    return old;
}

static inline bool os_atomic_load_bool(const volatile bool* ptr) {
    return *ptr;
}
