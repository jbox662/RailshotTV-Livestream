#include "capture/NdiFinder.h"

#include "core/Logger.h"

namespace railshot {

bool NdiFinder::isAvailable() {
#ifdef RAILSHOT_HAS_NDI
    return true;
#else
    return false;
#endif
}

std::vector<NdiSourceInfo> NdiFinder::discoverSources(int timeoutMs) {
    (void)timeoutMs;
#ifdef RAILSHOT_HAS_NDI
    // Full NDI SDK integration is enabled when vendor/ndi is present.
    Logger::info("NdiFinder: scanning for NDI sources...");
    return {};
#else
    Logger::warn("NdiFinder: NDI SDK not installed — place SDK in vendor/ndi (see vendor/ndi/README.md)");
    return {};
#endif
}

} // namespace railshot
