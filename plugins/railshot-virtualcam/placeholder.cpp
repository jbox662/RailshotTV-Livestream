#include <windows.h>
#include <stdint.h>

extern HINSTANCE dll_inst;

static bool initialized = false;

bool initialize_placeholder() {
    initialized = true;
    return false; // use solid gray placeholder
}

const uint8_t* get_placeholder_ptr() {
    return nullptr;
}

const bool get_placeholder_size(int* out_cx, int* out_cy) {
    if (out_cx) {
        *out_cx = 0;
    }
    if (out_cy) {
        *out_cy = 0;
    }
    return false;
}
