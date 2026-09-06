#include "DelayLine.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace beat::leveler
{

void DelayLine::prepare(std::size_t numChannels, std::size_t maxDelaySamples)
{
    if (numChannels == 0 || numChannels > 2)
        throw std::invalid_argument("DelayLine supports one or two channels");
    channels = numChannels;
    capacity = maxDelaySamples + 1;
    storage.assign(channels * capacity, 0.0f);
    delay = 0;
    writePosition = 0;
}

void DelayLine::setDelay(std::size_t delaySamples)
{
    if (delaySamples >= capacity)
        throw std::out_of_range("Delay exceeds prepared capacity");
    delay = delaySamples;
}

void DelayLine::reset() noexcept
{
    std::fill(storage.begin(), storage.end(), 0.0f);
    writePosition = 0;
}

void DelayLine::process(const float* const* input,
                        float* const* output,
                        std::size_t numSamples) noexcept
{
    assert(input != nullptr && output != nullptr);
    assert(channels > 0 && capacity > 0);
    if (numSamples == 0)
        return;

    for (std::size_t sample = 0; sample < numSamples; ++sample)
    {
        const auto readPosition = (writePosition + capacity - delay) % capacity;
        for (std::size_t channel = 0; channel < channels; ++channel)
        {
            const auto index = channel * capacity + writePosition;
            const auto delayed = storage[channel * capacity + readPosition];
            const auto current = input[channel][sample];
            storage[index] = current;
            output[channel][sample] = delay == 0 ? current : delayed;
        }
        writePosition = (writePosition + 1) % capacity;
    }
}

} // namespace beat::leveler
