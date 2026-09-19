#pragma once

#include "dsp/DetectionMetrics.h"

#include <filesystem>
#include <istream>
#include <vector>

namespace beat::leveler::harness
{

struct RealKitRecording
{
    std::filesystem::path relativeAudioPath;
    std::filesystem::path audioPath;
    std::vector<AnnotatedHit> annotations;
};

// Manifest format:
// file,onset_sample,peak_dbfs,kind
// kick-01.wav,4800,-12.0,kick
std::vector<RealKitRecording> readRealKitManifest(std::istream& stream,
                                                   const std::filesystem::path& root);
std::vector<RealKitRecording> readRealKitManifest(const std::filesystem::path& root);

const char* hitKindName(HitKind kind) noexcept;

} // namespace beat::leveler::harness
