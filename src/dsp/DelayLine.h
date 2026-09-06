#pragma once

#include <cstddef>
#include <vector>

namespace beat::leveler
{

class DelayLine
{
public:
    void prepare(std::size_t numChannels, std::size_t maxDelaySamples);
    void setDelay(std::size_t delaySamples);
    void reset() noexcept;

    std::size_t delaySamples() const noexcept { return delay; }
    std::size_t numChannels() const noexcept { return channels; }

    void process(const float* const* input,
                 float* const* output,
                 std::size_t numSamples) noexcept;

private:
    std::size_t channels = 0;
    std::size_t capacity = 0;
    std::size_t delay = 0;
    std::size_t writePosition = 0;
    std::vector<float> storage;
};

} // namespace beat::leveler
