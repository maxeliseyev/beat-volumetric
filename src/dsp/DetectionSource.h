#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace beat::leveler
{

// Which signal the detector and the level meter look at. The applied gain is
// always one scalar on every channel; this only chooses what it is measured on.
enum class DetectionSource
{
    // Power sum of both channels. An out-of-phase hit does not cancel.
    stereo,
    // (L+R)/2. Picks out centred hits and ignores wide cymbals and room, but
    // an out-of-phase hit cancels.
    mid,
    // Louder channel at each sample.
    peak
};

// Amplitude of one sample frame under the chosen source. `second` is unused for mono.
inline double combineChannels(DetectionSource source, std::size_t numChannels, double first, double second) noexcept
{
    if (numChannels == 1)
        return std::abs(first);
    switch (source)
    {
    case DetectionSource::mid:
        return std::abs(0.5 * (first + second));
    case DetectionSource::peak:
        return std::max(std::abs(first), std::abs(second));
    case DetectionSource::stereo:
        break;
    }
    return std::sqrt(0.5 * (first * first + second * second));
}

} // namespace beat::leveler
