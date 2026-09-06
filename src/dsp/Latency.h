#pragma once

#include <cstdint>
#include <stdexcept>

namespace beat::leveler
{

struct LatencyBudget
{
    double confirmationMs = 0.0;
    double measurementMs = 0.0;
    double placementMs = 0.0;
    double sampleRate = 48000.0;

    std::int64_t requiredSamples() const
    {
        if (sampleRate <= 0.0 || confirmationMs < 0.0 || measurementMs < 0.0 || placementMs < 0.0)
            throw std::invalid_argument("Latency budget values must be non-negative");
        const auto milliseconds = confirmationMs + measurementMs + placementMs;
        return static_cast<std::int64_t>(milliseconds * sampleRate / 1000.0 + 0.999999999);
    }
};

inline bool isReadyBeforeAttack(const OnsetEvent& event, std::int64_t outputSample, std::int64_t latency)
{
    return event.decisionReadySample <= event.onsetSample + latency
           && outputSample <= event.onsetSample + latency;
}

} // namespace beat::leveler
