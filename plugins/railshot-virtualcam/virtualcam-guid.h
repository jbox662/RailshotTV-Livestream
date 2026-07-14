#pragma once

#include <windows.h>
#include <initguid.h>

// Unique RailShot Virtual Camera CLSID — do not reuse OBS GUIDs.
DEFINE_GUID(CLSID_RailShot_VirtualVideo,
            0xA7F3C2E1,
            0x9B4D,
            0x4E8A,
            0xB1, 0xC6, 0x5D, 0x2F, 0x8A, 0x0E, 0x3C, 0x47);
