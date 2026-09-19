#include "StreamingLeveler.h"

#include "Latency.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace beat::leveler
{

namespace
{

constexpr float silenceDb = -240.0f;
constexpr float attackMs = 2.0f;
constexpr float releaseMs = 8.0f;

std::size_t msToSamples(double milliseconds, double sampleRate)
{
    return static_cast<std::size_t>(std::max(1.0, std::round(milliseconds * sampleRate / 1000.0)));
}

} // namespace

void StreamingLeveler::prepare(double sampleRate, std::size_t numChannels)
{
    if (sampleRate < 8000.0 || sampleRate > 192000.0 || numChannels == 0 || numChannels > 2)
        throw std::invalid_argument("Streaming leveler supports 8-192 kHz mono or stereo");

    channels = numChannels;
    const LatencyBudget budget { 4.0, 50.0, 2.0, sampleRate };
    latency = static_cast<std::size_t>(budget.requiredSamples());
    attackSamples = msToSamples(attackMs, sampleRate);
    releaseSamples = msToSamples(releaseMs, sampleRate);
    analyzer.prepare(sampleRate, numChannels);
    delayLine.prepare(numChannels, latency + 1);
    delayLine.setDelay(latency);
    reset();
}

void StreamingLeveler::reset(std::uint64_t epoch) noexcept
{
    analyzer.reset(epoch);
    delayLine.reset();
    for (auto& item : scheduled)
        item = {};
    targetHistory.fill(0.0f);
    targetScratch.fill(0.0f);
    targetCount = 0;
    targetWrite = 0;
    processedSamples = 0;
    lastGainDbValue = 0.0f;
    late = 0;
    dropped = 0;
}

float StreamingLeveler::toDb(float amplitude) noexcept
{
    if (!(amplitude > 1.0e-12f))
        return silenceDb;
    return 20.0f * std::log10(amplitude);
}

float StreamingLeveler::toLinear(float decibels) noexcept
{
    return std::pow(10.0f, decibels / 20.0f);
}

float StreamingLeveler::automaticTarget() noexcept
{
    if (targetCount == 0)
        return -12.0f;

    std::copy_n(targetHistory.begin(), targetCount, targetScratch.begin());
    auto middle = targetScratch.begin() + static_cast<std::ptrdiff_t>(targetCount / 2);
    std::nth_element(targetScratch.begin(), middle,
                     targetScratch.begin() + static_cast<std::ptrdiff_t>(targetCount));
    return *middle;
}

std::size_t StreamingLeveler::findScheduleSlot() noexcept
{
    for (std::size_t index = 0; index < scheduled.size(); ++index)
    {
        if (!scheduled[index].active)
            return index;
    }

    auto oldest = std::size_t { 0 };
    for (std::size_t index = 1; index < scheduled.size(); ++index)
    {
        if (scheduled[index].onset < scheduled[oldest].onset)
            oldest = index;
    }
    ++dropped;
    return oldest;
}

void StreamingLeveler::schedule(const HitMeasurement& measurement,
                                const LevelerParameters& parameters) noexcept
{
    const auto measured = measurement.weightedRms > 1.0e-12f ? measurement.weightedRms
                                                              : measurement.rms;
    const auto measuredDb = toDb(measured);
    if (measuredDb <= silenceDb)
        return;

    const auto target = parameters.automaticTarget ? automaticTarget() : parameters.targetDbfs;
    const auto strength = std::clamp(parameters.strength, 0.0f, 1.0f);
    const auto requestedDb = (target - measuredDb) * strength;
    const auto limitedDb = std::clamp(requestedDb,
                                      -std::max(0.0f, parameters.maxCutDb),
                                      std::max(0.0f, parameters.maxBoostDb));
    const auto slot = findScheduleSlot();
    const auto onset = measurement.event.onsetSample;
    const auto holdLength = static_cast<std::int64_t>(std::max<std::size_t>(1, measurement.windowSamples));
    scheduled[slot] = { std::max<std::int64_t>(0, onset - static_cast<std::int64_t>(attackSamples)),
                        onset,
                        onset + holdLength,
                        onset + holdLength + static_cast<std::int64_t>(releaseSamples),
                        toLinear(limitedDb),
                        true };
    lastGainDbValue = limitedDb;

    if (parameters.automaticTarget && measuredDb > silenceDb)
    {
        targetHistory[targetWrite] = measuredDb;
        targetWrite = (targetWrite + 1) % targetHistory.size();
        targetCount = std::min(targetCount + 1, targetHistory.size());
    }
}

float StreamingLeveler::gainAt(std::int64_t sourceSample) noexcept
{
    auto selected = scheduled.size();
    auto selectedOnset = std::numeric_limits<std::int64_t>::min();
    for (std::size_t index = 0; index < scheduled.size(); ++index)
    {
        auto& item = scheduled[index];
        if (!item.active)
            continue;
        if (sourceSample > item.end)
        {
            item.active = false;
            continue;
        }
        if (sourceSample >= item.start && item.onset >= selectedOnset)
        {
            selected = index;
            selectedOnset = item.onset;
        }
    }

    if (selected == scheduled.size())
        return 1.0f;

    const auto& item = scheduled[selected];
    if (sourceSample < item.onset)
    {
        const auto duration = std::max<std::int64_t>(1, item.onset - item.start);
        const auto position = static_cast<float>(sourceSample - item.start)
                              / static_cast<float>(duration);
        return 1.0f + (item.gain - 1.0f) * std::clamp(position, 0.0f, 1.0f);
    }
    if (sourceSample <= item.holdEnd)
        return item.gain;

    const auto duration = std::max<std::int64_t>(1, item.end - item.holdEnd);
    const auto position = static_cast<float>(sourceSample - item.holdEnd)
                          / static_cast<float>(duration);
    return item.gain + (1.0f - item.gain) * std::clamp(position, 0.0f, 1.0f);
}

std::size_t StreamingLeveler::process(const float* const* input,
                                      float* const* output,
                                      std::size_t numSamples,
                                      const LevelerParameters& parameters,
                                      std::span<OnsetEvent> events,
                                      std::span<HitMeasurement> measurements) noexcept
{
    assert(channels == 1 || channels == 2);
    assert(input != nullptr && output != nullptr);
    const auto measurementsWritten = analyzer.process(input, output, numSamples, events, measurements);
    for (std::size_t index = 0; index < measurementsWritten; ++index)
    {
        const auto& event = measurements[index].event;
        if (event.decisionReadySample > event.onsetSample + static_cast<std::int64_t>(latency))
        {
            ++late;
            continue;
        }
        schedule(measurements[index], parameters);
    }

    const auto blockStart = processedSamples;
    delayLine.process(input, output, numSamples);
    for (std::size_t sample = 0; sample < numSamples; ++sample)
    {
        const auto sourceSample = blockStart + static_cast<std::int64_t>(sample)
                                  - static_cast<std::int64_t>(latency);
        const auto gain = sourceSample >= 0 ? gainAt(sourceSample) : 1.0f;
        const auto mix = std::clamp(parameters.mix, 0.0f, 1.0f);
        const auto wetAmount = mix * (gain - 1.0f);
        for (std::size_t channel = 0; channel < channels; ++channel)
            output[channel][sample] *= 1.0f + wetAmount;
    }
    processedSamples += static_cast<std::int64_t>(numSamples);
    return measurementsWritten;
}

} // namespace beat::leveler
