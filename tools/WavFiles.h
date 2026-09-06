#pragma once

#include "Harness.h"
#include <filesystem>

namespace beat::leveler::harness
{

Audio readWav(const std::filesystem::path& path);
void writeWav(const std::filesystem::path& path, const Audio& audio);

} // namespace beat::leveler::harness
