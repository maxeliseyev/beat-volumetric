#pragma once

#include "Event.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace beat::leveler
{

struct StreamingDetectorConfig
{
    double sampleRate = 48000.0;
    double windowMs = 20.0;
    double hopMs = 5.0;
    double lowBandHz = 250.0;
    double highBandHz = 12000.0;
    double thresholdMultiplier = 3.0;
    double minimumFlux = 0.1;
    double refractoryMs = 100.0;
    std::size_t thresholdHistoryFrames = 63;
    std::size_t maxEventsPerBlock = 32;
};

// Bounded, causal spectral-flux detector. prepare() owns all allocation; process()
// accepts planar mono/stereo and emits timestamps on the input sample timeline.
class StreamingDetector
{
public:
    void prepare(const StreamingDetectorConfig& config);
    void reset(std::uint64_t newEpoch = 0) noexcept;

    std::size_t process(const float* const* input,
                        std::size_t numChannels,
                        std::size_t numSamples,
                        std::span<OnsetEvent> events) noexcept;

    std::size_t windowSamples() const noexcept { return window; }
    std::size_t hopSamples() const noexcept { return hop; }
    std::uint64_t droppedEvents() const noexcept { return dropped; }

private:
    void analyzeFrame(std::span<OnsetEvent> events, std::size_t& eventCount) noexcept;
    void fft() noexcept;
    float adaptiveThreshold() noexcept;

    StreamingDetectorConfig settings;
    std::size_t window = 0;
    std::size_t hop = 0;
    std::size_t fftSize = 0;
    std::size_t firstBin = 1;
    std::size_t lastBin = 1;
    std::size_t lowLastBin = 1;
    std::size_t write = 0;
    std::size_t filled = 0;
    std::size_t samplesSinceFrame = 0;
    std::size_t historyCount = 0;
    std::size_t historyWrite = 0;
    std::int64_t nextSample = 0;
    std::int64_t lastOnset = -1;
    std::uint64_t epoch = 0;
    std::uint64_t nextId = 1;
    std::uint64_t dropped = 0;
    std::vector<float> inputRing;
    std::vector<float> hann;
    std::vector<float> real;
    std::vector<float> imaginary;
    std::vector<float> previousMagnitude;
    std::vector<float> fluxHistory;
    std::vector<float> medianScratch;
};

} // namespace beat::leveler
