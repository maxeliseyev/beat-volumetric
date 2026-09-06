#include "WavFiles.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace beat::leveler::harness
{

namespace
{

juce::File toFile(const std::filesystem::path& path)
{
    const auto utf8 = std::filesystem::absolute(path).u8string();
    return juce::File(juce::String::fromUTF8(reinterpret_cast<const char*>(utf8.c_str())));
}

} // namespace

Audio readWav(const std::filesystem::path& path)
{
    juce::WavAudioFormat format;
    auto stream = toFile(path).createInputStream();
    if (!stream)
        throw std::runtime_error("Cannot open input WAV");
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    if (!reader || reader->numChannels < 1 || reader->numChannels > 2
        || reader->sampleRate < 8000 || reader->sampleRate > 192000
        || reader->sampleRate != std::floor(reader->sampleRate)
        || reader->lengthInSamples < 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
        throw std::runtime_error("Expected a mono/stereo WAV at 8000..192000 Hz, < 2^31 frames");

    Audio audio { static_cast<int>(reader->sampleRate),
                  std::vector<std::vector<float>>(reader->numChannels,
                      std::vector<float>(static_cast<std::size_t>(reader->lengthInSamples))) };
    std::array<float*, 2> channels {};
    for (std::size_t channel = 0; channel < audio.channels.size(); ++channel)
        channels[channel] = audio.channels[channel].data();
    if (audio.frames() != 0
        && !reader->read(channels.data(), static_cast<int>(audio.channels.size()), 0,
                         static_cast<int>(audio.frames())))
        throw std::runtime_error("Failed to decode input WAV");
    audio.validate();
    return audio;
}

void writeWav(const std::filesystem::path& path, const Audio& audio)
{
    audio.validate();
    if (audio.frames() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("WAV exceeds runner frame limit");
    // The CLI rejects existing paths as well; keep the library safe for other callers.
    if (std::filesystem::exists(path))
        throw std::runtime_error("Output already exists: " + path.string());
    auto fileStream = toFile(path).createOutputStream();
    if (!fileStream || !fileStream->openedOk())
        throw std::runtime_error("Cannot open output WAV");
    std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);
    const auto options = juce::AudioFormatWriterOptions()
        .withSampleRate(audio.sampleRate)
        .withNumChannels(static_cast<int>(audio.channels.size()))
        .withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, options);
    if (!writer)
        throw std::runtime_error("Cannot create float WAV writer");
    std::array<const float*, 2> channels {};
    for (std::size_t channel = 0; channel < audio.channels.size(); ++channel)
        channels[channel] = audio.channels[channel].data();
    if (!writer->writeFromFloatArrays(channels.data(), static_cast<int>(audio.channels.size()),
                                      static_cast<int>(audio.frames()))
        || !writer->flush())
        throw std::runtime_error("Failed to write output WAV");
}

} // namespace beat::leveler::harness
