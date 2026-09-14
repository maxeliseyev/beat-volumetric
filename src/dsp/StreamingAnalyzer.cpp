#include "StreamingAnalyzer.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace beat::leveler
{

void StreamingAnalyzer::prepare(double sampleRate, std::size_t numChannels)
{
    if (numChannels == 0 || numChannels > 2)
        throw std::invalid_argument("Streaming analyzer supports mono or stereo");
    channels = numChannels;
    StreamingDetectorConfig detectorConfig;
    detectorConfig.sampleRate = sampleRate;
    detector.prepare(detectorConfig);
    HitLevelMeterConfig meterConfig;
    meterConfig.sampleRate = sampleRate;
    meterConfig.historySamples = std::max<std::size_t>(8192, detector.windowSamples() * 4);
    meter.prepare(meterConfig);
}

void StreamingAnalyzer::reset(std::uint64_t epoch) noexcept
{
    detector.reset(epoch);
    meter.reset();
}

std::size_t StreamingAnalyzer::process(const float* const* input, float* const* output, std::size_t numSamples,
                                       std::span<OnsetEvent> events,
                                       std::span<HitMeasurement> measurements) noexcept
{
    assert(channels > 0);
    passthrough.process(input, output, channels, numSamples);
    const auto eventCount = detector.process(input, channels, numSamples, events);
    return meter.process(input, channels, numSamples, events.first(eventCount), measurements);
}

} // namespace beat::leveler
