#pragma once

#include "DelayLine.h"
#include "StreamingAnalyzer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace beat::leveler
{

struct LevelerParameters
{
    float strength = 0.5f;
    bool automaticTarget = true;
    float targetDbfs = -12.0f;
    float mix = 1.0f;
    float maxBoostDb = 6.0f;
    float maxCutDb = 12.0f;
};

// Causal beat leveler with a fixed lookahead. Detection and measurement happen
// on the input timeline; the delay line keeps the corresponding attack in the
// future long enough for its gain decision to be scheduled.
class StreamingLeveler
{
public:
    void prepare(double sampleRate, std::size_t numChannels);
    void reset(std::uint64_t epoch = 0) noexcept;

    std::size_t process(const float* const* input,
                        float* const* output,
                        std::size_t numSamples,
                        const LevelerParameters& parameters,
                        std::span<OnsetEvent> events,
                        std::span<HitMeasurement> measurements) noexcept;

    std::size_t latencySamples() const noexcept { return latency; }
    float lastGainDb() const noexcept { return lastGainDbValue; }
    std::uint64_t lateEvents() const noexcept { return late; }
    std::uint64_t droppedSchedules() const noexcept { return dropped; }

private:
    struct ScheduledGain
    {
        std::int64_t start = 0;
        std::int64_t onset = 0;
        std::int64_t holdEnd = 0;
        std::int64_t end = 0;
        float gain = 1.0f;
        bool active = false;
    };

    static float toDb(float amplitude) noexcept;
    static float toLinear(float decibels) noexcept;

    void schedule(const HitMeasurement& measurement, const LevelerParameters& parameters) noexcept;
    float automaticTarget() noexcept;
    float gainAt(std::int64_t sourceSample) noexcept;
    std::size_t findScheduleSlot() noexcept;

    std::size_t channels = 0;
    std::size_t latency = 0;
    std::size_t attackSamples = 0;
    std::size_t releaseSamples = 0;
    std::int64_t processedSamples = 0;
    StreamingAnalyzer analyzer;
    DelayLine delayLine;
    std::array<ScheduledGain, 64> scheduled {};
    std::array<float, 31> targetHistory {};
    std::array<float, 31> targetScratch {};
    std::size_t targetCount = 0;
    std::size_t targetWrite = 0;
    float lastGainDbValue = 0.0f;
    std::uint64_t late = 0;
    std::uint64_t dropped = 0;
};

} // namespace beat::leveler
