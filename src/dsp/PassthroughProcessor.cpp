#include "PassthroughProcessor.h"

#include <algorithm>
#include <cassert>

namespace beat::leveler
{

void PassthroughProcessor::process(const float* const* input,
                                   float* const* output,
                                   std::size_t numChannels,
                                   std::size_t numSamples) noexcept
{
    assert(numChannels == 1 || numChannels == 2);
    if (numSamples == 0)
        return;

    for (std::size_t channel = 0; channel < numChannels; ++channel)
    {
        if (input[channel] != output[channel])
            std::copy_n(input[channel], numSamples, output[channel]);
    }
}

} // namespace beat::leveler
