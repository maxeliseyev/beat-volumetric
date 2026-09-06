#pragma once

#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

namespace beat::leveler::harness
{

// File-side storage, never allocated inside the realtime processor.
struct Audio
{
    int sampleRate = 48000;
    std::vector<std::vector<float>> channels;

    std::size_t frames() const;
    void validate() const;
};

struct KnownHit
{
    std::int64_t onsetSample = 0;
    std::int64_t lengthSamples = 0;
    double peakDb = 0.0;
};

struct Fixture
{
    Audio audio;
    std::vector<KnownHit> hits;
};

Fixture makeSynthetic(int sampleRate, std::size_t numChannels);
Audio render(const Audio& input, std::span<const std::size_t> blockPattern);
void writeReport(std::ostream& stream,
                 const Audio& input,
                 const Audio& output,
                 std::span<const KnownHit> hits);

} // namespace beat::leveler::harness
