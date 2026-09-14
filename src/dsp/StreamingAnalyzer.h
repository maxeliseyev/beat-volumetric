#pragma once

#include "HitLevelMeter.h"
#include "PassthroughProcessor.h"
#include "StreamingDetector.h"

#include <span>

namespace beat::leveler
{

class StreamingAnalyzer
{
public:
    void prepare(double sampleRate, std::size_t numChannels);
    void reset(std::uint64_t epoch = 0) noexcept;
    std::size_t process(const float* const* input, float* const* output, std::size_t numSamples,
                        std::span<OnsetEvent> events, std::span<HitMeasurement> measurements) noexcept;

private:
    std::size_t channels = 0;
    PassthroughProcessor passthrough;
    StreamingDetector detector;
    HitLevelMeter meter;
};

} // namespace beat::leveler
