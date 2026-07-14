#pragma once

#include <string>

namespace railshot {

// Remux/transcontainer without re-encoding. Returns true on success.
bool remuxFile(const std::string& inputPath, const std::string& outputPath);

} // namespace railshot
