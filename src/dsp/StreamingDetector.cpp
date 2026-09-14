#include "StreamingDetector.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace beat::leveler
{

namespace
{

std::size_t msToSamples(double milliseconds, double sampleRate)
{
    return static_cast<std::size_t>(std::max(1.0, std::round(milliseconds * sampleRate / 1000.0)));
}

std::size_t nextPowerOfTwo(std::size_t value)
{
    std::size_t result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

} // namespace

void StreamingDetector::prepare(const StreamingDetectorConfig& config)
{
    if (config.sampleRate < 8000.0 || config.sampleRate > 192000.0 || config.windowMs <= 0.0
        || config.hopMs <= 0.0 || config.lowBandHz <= 0.0 || config.highBandHz <= config.lowBandHz
        || config.thresholdMultiplier < 0.0 || config.minimumFlux < 0.0 || config.refractoryMs < 0.0
        || config.thresholdHistoryFrames == 0 || config.maxEventsPerBlock == 0)
        throw std::invalid_argument("Invalid streaming detector configuration");

    settings = config;
    window = msToSamples(config.windowMs, config.sampleRate);
    hop = msToSamples(config.hopMs, config.sampleRate);
    fftSize = nextPowerOfTwo(window);
    const auto binHz = config.sampleRate / static_cast<double>(fftSize);
    firstBin = std::max<std::size_t>(1, static_cast<std::size_t>(std::floor(config.lowBandHz / binHz)));
    lastBin = std::min(fftSize / 2, static_cast<std::size_t>(std::ceil(config.highBandHz / binHz)));
    lowLastBin = std::clamp(static_cast<std::size_t>(std::ceil(config.lowBandHz / binHz)), firstBin, lastBin);
    if (lastBin <= firstBin)
        throw std::invalid_argument("Detector band does not contain enough FFT bins");

    inputRing.assign(window, 0.0f);
    hann.resize(window);
    for (std::size_t index = 0; index < window; ++index)
    {
        const auto phase = 2.0 * std::numbers::pi * static_cast<double>(index)
                           / static_cast<double>(std::max<std::size_t>(1, window - 1));
        hann[index] = static_cast<float>(0.5 - 0.5 * std::cos(phase));
    }
    real.assign(fftSize, 0.0f);
    imaginary.assign(fftSize, 0.0f);
    previousMagnitude.assign(fftSize / 2 + 1, 0.0f);
    fluxHistory.assign(config.thresholdHistoryFrames, 0.0f);
    medianScratch.assign(config.thresholdHistoryFrames, 0.0f);
    reset();
}

void StreamingDetector::reset(std::uint64_t newEpoch) noexcept
{
    std::fill(inputRing.begin(), inputRing.end(), 0.0f);
    std::fill(previousMagnitude.begin(), previousMagnitude.end(), 0.0f);
    std::fill(fluxHistory.begin(), fluxHistory.end(), 0.0f);
    write = 0;
    filled = 0;
    samplesSinceFrame = 0;
    historyCount = 0;
    historyWrite = 0;
    nextSample = 0;
    lastOnset = -1;
    epoch = newEpoch;
    nextId = 1;
    dropped = 0;
}

std::size_t StreamingDetector::process(const float* const* input,
                                       std::size_t numChannels,
                                       std::size_t numSamples,
                                       std::span<OnsetEvent> events) noexcept
{
    assert(numChannels == 1 || numChannels == 2);
    assert(numSamples == 0 || input != nullptr);
    std::size_t eventCount = 0;
    for (std::size_t sample = 0; sample < numSamples; ++sample)
    {
        double energy = 0.0;
        for (std::size_t channel = 0; channel < numChannels; ++channel)
        {
            const auto value = static_cast<double>(input[channel][sample]);
            energy += value * value;
        }
        inputRing[write] = static_cast<float>(std::sqrt(energy / static_cast<double>(numChannels)));
        write = (write + 1) % window;
        filled = std::min(window, filled + 1);
        ++samplesSinceFrame;
        ++nextSample;
        if (filled == window && samplesSinceFrame >= hop)
        {
            samplesSinceFrame = 0;
            analyzeFrame(events, eventCount);
        }
    }
    return eventCount;
}

void StreamingDetector::fft() noexcept
{
    for (std::size_t index = 1, reversed = 0; index < fftSize; ++index)
    {
        std::size_t bit = fftSize >> 1;
        for (; (reversed & bit) != 0; bit >>= 1)
            reversed ^= bit;
        reversed ^= bit;
        if (index < reversed)
        {
            std::swap(real[index], real[reversed]);
            std::swap(imaginary[index], imaginary[reversed]);
        }
    }
    for (std::size_t length = 2; length <= fftSize; length <<= 1)
    {
        const auto angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const auto unitReal = static_cast<float>(std::cos(angle));
        const auto unitImaginary = static_cast<float>(std::sin(angle));
        for (std::size_t start = 0; start < fftSize; start += length)
        {
            float twiddleReal = 1.0f;
            float twiddleImaginary = 0.0f;
            for (std::size_t offset = 0; offset < length / 2; ++offset)
            {
                const auto even = start + offset;
                const auto odd = even + length / 2;
                const auto oddReal = real[odd] * twiddleReal - imaginary[odd] * twiddleImaginary;
                const auto oddImaginary = real[odd] * twiddleImaginary + imaginary[odd] * twiddleReal;
                const auto evenReal = real[even];
                const auto evenImaginary = imaginary[even];
                real[even] = evenReal + oddReal;
                imaginary[even] = evenImaginary + oddImaginary;
                real[odd] = evenReal - oddReal;
                imaginary[odd] = evenImaginary - oddImaginary;
                const auto nextReal = twiddleReal * unitReal - twiddleImaginary * unitImaginary;
                twiddleImaginary = twiddleReal * unitImaginary + twiddleImaginary * unitReal;
                twiddleReal = nextReal;
            }
        }
    }
}

float StreamingDetector::adaptiveThreshold() noexcept
{
    if (historyCount == 0)
        return static_cast<float>(settings.minimumFlux);
    std::copy_n(fluxHistory.begin(), historyCount, medianScratch.begin());
    auto middle = medianScratch.begin() + static_cast<std::ptrdiff_t>(historyCount / 2);
    std::nth_element(medianScratch.begin(), middle, medianScratch.begin() + static_cast<std::ptrdiff_t>(historyCount));
    const auto median = *middle;
    return std::max(static_cast<float>(settings.minimumFlux), median * static_cast<float>(settings.thresholdMultiplier));
}

void StreamingDetector::analyzeFrame(std::span<OnsetEvent> events, std::size_t& eventCount) noexcept
{
    std::fill(real.begin(), real.end(), 0.0f);
    std::fill(imaginary.begin(), imaginary.end(), 0.0f);
    for (std::size_t index = 0; index < window; ++index)
        real[index] = inputRing[(write + index) % window] * hann[index];
    fft();

    float flux = 0.0f;
    float lowFlux = 0.0f;
    for (std::size_t bin = firstBin; bin <= lastBin; ++bin)
    {
        const auto magnitude = std::sqrt(real[bin] * real[bin] + imaginary[bin] * imaginary[bin]);
        const auto increase = std::max(0.0f, std::log1p(1000.0f * magnitude)
                                                 - std::log1p(1000.0f * previousMagnitude[bin]));
        flux += increase;
        if (bin <= lowLastBin)
            lowFlux += increase;
        previousMagnitude[bin] = magnitude;
    }

    const auto threshold = adaptiveThreshold();
    std::size_t peakIndex = 0;
    for (std::size_t index = 1; index < window; ++index)
    {
        if (inputRing[(write + index) % window] > inputRing[(write + peakIndex) % window])
            peakIndex = index;
    }
    const auto onset = nextSample - static_cast<std::int64_t>(window)
                       + static_cast<std::int64_t>(peakIndex);
    const auto refractory = static_cast<std::int64_t>(msToSamples(settings.refractoryMs, settings.sampleRate));
    if (flux >= threshold && (lastOnset < 0 || onset - lastOnset >= refractory))
    {
        if (eventCount < events.size())
        {
            const auto excess = std::max(0.0f, flux - threshold);
            const auto confidence = std::clamp(excess / std::max(flux, 1.0e-12f)
                                               + 0.25f * lowFlux / std::max(flux, 1.0e-12f),
                                               0.0f, 1.0f);
            events[eventCount++] = { epoch, nextId++, onset, nextSample, confidence,
                                     EventProvenance::detector };
            lastOnset = onset;
        }
        else
        {
            ++dropped;
        }
    }
    fluxHistory[historyWrite] = flux;
    historyWrite = (historyWrite + 1) % fluxHistory.size();
    historyCount = std::min(historyCount + 1, fluxHistory.size());
}

} // namespace beat::leveler
