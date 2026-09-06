#pragma once

#include <cstdint>

namespace beat::leveler
{

enum class EventProvenance : std::uint8_t
{
    detector,
    templateMatch,
    model,
    user
};

struct OnsetEvent
{
    std::uint64_t epoch = 0;
    std::uint64_t id = 0;
    std::int64_t onsetSample = 0;
    std::int64_t decisionReadySample = 0;
    float confidence = 0.0f;
    EventProvenance provenance = EventProvenance::detector;
};

} // namespace beat::leveler
