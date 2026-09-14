#include "DetectionMetrics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace beat::leveler
{

DetectionMetrics evaluateDetections(std::span<const AnnotatedHit> annotations,
                                    std::span<const OnsetEvent> detections,
                                    std::span<const HitMeasurement> measurements,
                                    std::int64_t toleranceSamples)
{
    if (toleranceSamples < 0)
        throw std::invalid_argument("Detection match tolerance must be non-negative");

    DetectionMetrics result;
    result.detected = detections.size();
    for (const auto& annotation : annotations)
        ++result.byKind[static_cast<std::size_t>(annotation.kind)].expected;

    std::vector<bool> used(detections.size(), false);
    std::array<double, static_cast<std::size_t>(HitKind::count)> timingSums {};
    std::array<double, static_cast<std::size_t>(HitKind::count)> levelSums {};
    for (const auto& annotation : annotations)
    {
        std::size_t closest = detections.size();
        auto distance = std::numeric_limits<std::int64_t>::max();
        for (std::size_t index = 0; index < detections.size(); ++index)
        {
            if (used[index])
                continue;
            const auto candidate = std::llabs(detections[index].onsetSample - annotation.onsetSample);
            if (candidate <= toleranceSamples && candidate < distance)
            {
                closest = index;
                distance = candidate;
            }
        }
        if (closest == detections.size())
            continue;

        used[closest] = true;
        ++result.matched;
        const auto kind = static_cast<std::size_t>(annotation.kind);
        auto& metrics = result.byKind[kind];
        ++metrics.matched;
        timingSums[kind] += static_cast<double>(distance);
        const auto measurement = std::find_if(measurements.begin(), measurements.end(),
                                              [&](const auto& item) { return item.event.id == detections[closest].id; });
        if (measurement != measurements.end() && measurement->peak > 0.0f)
        {
            ++metrics.levelMeasurements;
            levelSums[kind] += std::abs(20.0 * std::log10(static_cast<double>(measurement->peak))
                                        - static_cast<double>(annotation.peakDbfs));
        }
    }
    result.falsePositives = result.detected - result.matched;
    result.falseNegatives = annotations.size() - result.matched;
    for (std::size_t kind = 0; kind < result.byKind.size(); ++kind)
    {
        auto& metrics = result.byKind[kind];
        if (metrics.matched != 0)
            metrics.meanAbsoluteTimingSamples = timingSums[kind] / static_cast<double>(metrics.matched);
        if (metrics.levelMeasurements != 0)
            metrics.meanAbsoluteLevelErrorDb = levelSums[kind] / static_cast<double>(metrics.levelMeasurements);
    }
    return result;
}

} // namespace beat::leveler
