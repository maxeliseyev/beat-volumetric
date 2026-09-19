#pragma once

#include "RealKitManifest.h"

#include <filesystem>
#include <iosfwd>
#include <span>
#include <vector>

namespace beat::leveler::harness
{

struct RealKitFileResult
{
    std::filesystem::path relativeAudioPath;
    int sampleRate = 0;
    std::size_t frames = 0;
    DetectionMetrics metrics;
};

struct RealKitRunResult
{
    std::vector<RealKitFileResult> files;
    DetectionMetrics overall;
};

RealKitRunResult runRealKit(const std::filesystem::path& root,
                            std::span<const std::size_t> blockPattern,
                            double toleranceMilliseconds);

void writeRealKitReport(std::ostream& stream, const RealKitRunResult& result);

} // namespace beat::leveler::harness
