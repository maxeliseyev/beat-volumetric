#pragma once

#include "Event.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace beat::leveler
{

struct HitMeasurement
{
    OnsetEvent event;
    float peak = 0.0f;
    float rms = 0.0f;
    float weightedRms = 0.0f;
    std::size_t windowSamples = 0;
};

struct HitLevelMeterConfig
{
    double sampleRate = 48000.0;
    double windowMs = 30.0;
    double weightedHighPassHz = 120.0;
    std::size_t historySamples = 8192;
    std::size_t maxPendingEvents = 64;
};

// Measures a common stereo level from a bounded input history. weightedRms is
// RMS after a first-order 120 Hz high-pass; output audio is never filtered.
class HitLevelMeter
{
public:
    void prepare(const HitLevelMeterConfig& config);
    void reset() noexcept;
    std::size_t process(const float* const* input,
                        std::size_t numChannels,
                        std::size_t numSamples,
                        std::span<const OnsetEvent> newEvents,
                        std::span<HitMeasurement> measurements) noexcept;

    std::size_t windowSamples() const noexcept { return window; }
    std::uint64_t droppedEvents() const noexcept { return dropped; }

private:
    struct Pending
    {
        OnsetEvent event;
        bool active = false;
    };

    bool emit(const OnsetEvent& event, std::span<HitMeasurement> measurements,
              std::size_t& measurementCount) noexcept;

    HitLevelMeterConfig settings;
    std::size_t window = 0;
    std::size_t write = 0;
    std::size_t filled = 0;
    std::int64_t nextSample = 0;
    std::array<float, 2> highPassInput {};
    std::array<float, 2> highPassOutput {};
    float highPassCoefficient = 0.0f;
    std::uint64_t dropped = 0;
    std::vector<float> amplitudeHistory;
    std::vector<float> weightedHistory;
    std::vector<Pending> pending;
};

} // namespace beat::leveler
