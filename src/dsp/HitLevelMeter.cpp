#include "HitLevelMeter.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace beat::leveler
{

void HitLevelMeter::prepare(const HitLevelMeterConfig& config)
{
    if (config.sampleRate < 8000.0 || config.sampleRate > 192000.0 || config.windowMs <= 0.0
        || config.weightedHighPassHz <= 0.0 || config.historySamples == 0 || config.maxPendingEvents == 0)
        throw std::invalid_argument("Invalid hit level meter configuration");
    settings = config;
    window = static_cast<std::size_t>(std::max(1.0, std::round(config.windowMs * config.sampleRate / 1000.0)));
    if (config.historySamples < window)
        throw std::invalid_argument("Level meter history is shorter than its measurement window");
    const auto decay = std::exp(-2.0 * std::numbers::pi * config.weightedHighPassHz / config.sampleRate);
    highPassCoefficient = static_cast<float>(decay);
    amplitudeHistory.assign(config.historySamples, 0.0f);
    weightedHistory.assign(config.historySamples, 0.0f);
    pending.assign(config.maxPendingEvents, {});
    reset();
}

void HitLevelMeter::reset() noexcept
{
    std::fill(amplitudeHistory.begin(), amplitudeHistory.end(), 0.0f);
    std::fill(weightedHistory.begin(), weightedHistory.end(), 0.0f);
    std::fill(pending.begin(), pending.end(), Pending {});
    write = 0;
    filled = 0;
    nextSample = 0;
    highPassInput.fill(0.0f);
    highPassOutput.fill(0.0f);
    dropped = 0;
}

bool HitLevelMeter::emit(const OnsetEvent& event, std::span<HitMeasurement> measurements,
                         std::size_t& measurementCount) noexcept
{
    const auto start = event.onsetSample;
    const auto end = start + static_cast<std::int64_t>(window);
    if (start < nextSample - static_cast<std::int64_t>(amplitudeHistory.size()) || end > nextSample)
        return false;
    if (measurementCount == measurements.size())
    {
        ++dropped;
        return true;
    }
    double sumSquares = 0.0;
    double weightedSquares = 0.0;
    float peak = 0.0f;
    const auto capacity = static_cast<std::int64_t>(amplitudeHistory.size());
    for (std::int64_t sample = start; sample < end; ++sample)
    {
        const auto index = static_cast<std::size_t>(sample % capacity);
        const auto amplitude = amplitudeHistory[index];
        peak = std::max(peak, amplitude);
        sumSquares += static_cast<double>(amplitude) * amplitude;
        const auto weighted = weightedHistory[index];
        weightedSquares += static_cast<double>(weighted) * weighted;
    }
    const auto divisor = static_cast<double>(window);
    measurements[measurementCount++] = { event, peak, static_cast<float>(std::sqrt(sumSquares / divisor)),
                                          static_cast<float>(std::sqrt(weightedSquares / divisor)), window };
    return true;
}

std::size_t HitLevelMeter::process(const float* const* input,
                                   std::size_t numChannels,
                                   std::size_t numSamples,
                                   std::span<const OnsetEvent> newEvents,
                                   std::span<HitMeasurement> measurements) noexcept
{
    assert(numChannels == 1 || numChannels == 2);
    assert(numSamples == 0 || input != nullptr);
    for (std::size_t sample = 0; sample < numSamples; ++sample)
    {
        double energy = 0.0;
        double weightedEnergy = 0.0;
        for (std::size_t channel = 0; channel < numChannels; ++channel)
        {
            const auto value = static_cast<double>(input[channel][sample]);
            energy += value * value;
            const auto weighted = input[channel][sample] - highPassInput[channel]
                                  + highPassCoefficient * highPassOutput[channel];
            highPassInput[channel] = input[channel][sample];
            highPassOutput[channel] = weighted;
            weightedEnergy += static_cast<double>(weighted) * weighted;
        }
        const auto amplitude = static_cast<float>(std::sqrt(energy / static_cast<double>(numChannels)));
        const auto weighted = static_cast<float>(std::sqrt(weightedEnergy / static_cast<double>(numChannels)));
        amplitudeHistory[write] = amplitude;
        weightedHistory[write] = weighted;
        write = (write + 1) % amplitudeHistory.size();
        filled = std::min(filled + 1, amplitudeHistory.size());
        ++nextSample;
    }

    std::size_t measurementCount = 0;
    for (auto& item : pending)
    {
        if (item.active && emit(item.event, measurements, measurementCount))
            item.active = false;
    }
    for (const auto& event : newEvents)
    {
        if (emit(event, measurements, measurementCount))
            continue;
        const auto slot = std::find_if(pending.begin(), pending.end(), [](const auto& item) { return !item.active; });
        if (slot == pending.end())
            ++dropped;
        else
            *slot = { event, true };
    }
    return measurementCount;
}

} // namespace beat::leveler
