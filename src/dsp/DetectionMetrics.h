#pragma once

#include "Event.h"
#include "HitLevelMeter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace beat::leveler
{

enum class HitKind : std::uint8_t
{
    kick,
    snare,
    tom,
    ghost,
    flam,
    bleed,
    count
};

struct AnnotatedHit
{
    std::int64_t onsetSample = 0;
    float peakDbfs = -240.0f;
    HitKind kind = HitKind::kick;
};

struct HitMatchMetrics
{
    std::size_t expected = 0;
    std::size_t matched = 0;
    std::size_t levelMeasurements = 0;
    double meanAbsoluteTimingSamples = 0.0;
    double meanAbsoluteLevelErrorDb = 0.0;
};

struct DetectionMetrics
{
    std::size_t detected = 0;
    std::size_t matched = 0;
    std::size_t falsePositives = 0;
    std::size_t falseNegatives = 0;
    std::array<HitMatchMetrics, static_cast<std::size_t>(HitKind::count)> byKind {};
};

// Greedily matches each annotated attack with the closest unmatched detection
// inside toleranceSamples. Precision remains global because this detector has no
// instrument classifier; recall/timing/level error stay separated by annotation kind.
DetectionMetrics evaluateDetections(std::span<const AnnotatedHit> annotations,
                                    std::span<const OnsetEvent> detections,
                                    std::span<const HitMeasurement> measurements,
                                    std::int64_t toleranceSamples);

} // namespace beat::leveler
