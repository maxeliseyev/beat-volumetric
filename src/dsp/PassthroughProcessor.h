#pragma once

#include <cstddef>

namespace beat::leveler
{

// Baseline for the file harness. The leveler and its latency contract follow in PR 02.
class PassthroughProcessor
{
public:
    // Planar mono/stereo; each channel may alias its own input exactly, or be disjoint.
    // Buffers must contain numSamples values. Null pointers are allowed for empty blocks.
    void process(const float* const* input,
                 float* const* output,
                 std::size_t numChannels,
                 std::size_t numSamples) noexcept;
};

} // namespace beat::leveler
