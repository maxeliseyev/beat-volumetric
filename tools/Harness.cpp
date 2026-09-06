#include "Harness.h"
#include "dsp/PassthroughProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <numbers>
#include <ostream>
#include <stdexcept>

namespace beat::leveler::harness
{

std::size_t Audio::frames() const
{
    return channels.empty() ? 0 : channels.front().size();
}

void Audio::validate() const
{
    if (sampleRate < 8000 || sampleRate > 192000 || channels.empty() || channels.size() > 2)
        throw std::invalid_argument("Expected mono/stereo at 8000..192000 Hz");

    for (const auto& channel : channels)
    {
        if (channel.size() != frames())
            throw std::invalid_argument("Channel lengths differ");
        if (!std::all_of(channel.begin(), channel.end(), [](float x) { return std::isfinite(x); }))
            throw std::invalid_argument("Audio contains NaN or infinity");
    }
}

Fixture makeSynthetic(int sampleRate, std::size_t numChannels)
{
    Fixture fixture;
    fixture.audio.sampleRate = sampleRate;
    if (numChannels == 0 || numChannels > 2)
        throw std::invalid_argument("Expected one or two channels");
    fixture.audio.channels.resize(numChannels);
    fixture.audio.validate();

    const auto frames = static_cast<std::size_t>(sampleRate) * 2;
    for (auto& channel : fixture.audio.channels)
        channel.assign(frames, 0.0f);

    constexpr std::array levels { -24.0, -12.0, -30.0, -6.0 };
    const auto length = static_cast<std::int64_t>(std::llround(sampleRate * 0.08));
    for (std::size_t hit = 0; hit < levels.size(); ++hit)
    {
        const auto onset = static_cast<std::int64_t>(
            std::llround(sampleRate * (0.1 + 0.4 * static_cast<double>(hit))));
        fixture.hits.push_back({ onset, length, levels[hit] });
        const auto amplitude = std::pow(10.0, levels[hit] / 20.0);
        for (std::int64_t i = 0; i < length; ++i)
        {
            const auto t = static_cast<double>(i) / sampleRate;
            const auto taper = 1.0 - static_cast<double>(i) / static_cast<double>(length - 1);
            const auto sample = static_cast<float>(
                amplitude * std::exp(-t / 0.018) * taper
                * std::cos(2.0 * std::numbers::pi * 140.0 * t));
            const auto position = static_cast<std::size_t>(onset + i);
            fixture.audio.channels[0][position] = sample;
            if (numChannels == 2)
                fixture.audio.channels[1][position] = -0.5f * sample;
        }
    }
    return fixture;
}

Audio render(const Audio& input, std::span<const std::size_t> blockPattern)
{
    input.validate();
    if (blockPattern.empty()
        || std::any_of(blockPattern.begin(), blockPattern.end(), [](auto size) { return size == 0; }))
        throw std::invalid_argument("Block pattern must contain positive sizes");

    Audio output { input.sampleRate,
                   std::vector<std::vector<float>>(input.channels.size(),
                                                   std::vector<float>(input.frames())) };
    PassthroughProcessor processor;
    std::array<const float*, 2> source {};
    std::array<float*, 2> destination {};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < input.frames())
    {
        const auto count = std::min(blockPattern[block], input.frames() - offset);
        for (std::size_t channel = 0; channel < input.channels.size(); ++channel)
        {
            source[channel] = input.channels[channel].data() + offset;
            destination[channel] = output.channels[channel].data() + offset;
        }
        processor.process(source.data(), destination.data(), input.channels.size(), count);
        offset += count;
        block = (block + 1) % blockPattern.size();
    }
    return output;
}

namespace
{

double db(double amplitude)
{
    // A finite report floor; silence remains zero in the audio itself.
    return 20.0 * std::log10(std::max(amplitude, 1.0e-12));
}

void writeRow(std::ostream& stream,
              const Audio& input,
              const Audio& output,
              std::size_t channel,
              std::size_t start,
              std::size_t length)
{
    double inPeak = 0.0, outPeak = 0.0, inEnergy = 0.0, outEnergy = 0.0, maxError = 0.0;
    for (std::size_t i = start; i < start + length; ++i)
    {
        const auto in = static_cast<double>(input.channels[channel][i]);
        const auto out = static_cast<double>(output.channels[channel][i]);
        inPeak = std::max(inPeak, std::abs(in));
        outPeak = std::max(outPeak, std::abs(out));
        inEnergy += in * in;
        outEnergy += out * out;
        maxError = std::max(maxError, std::abs(out - in));
    }
    const auto divisor = static_cast<double>(std::max(std::size_t { 1 }, length));
    stream << ',' << db(inPeak) << ',' << db(outPeak)
           << ',' << db(std::sqrt(inEnergy / divisor))
           << ',' << db(std::sqrt(outEnergy / divisor)) << ',' << maxError << '\n';
}

} // namespace

void writeReport(std::ostream& stream,
                 const Audio& input,
                 const Audio& output,
                 std::span<const KnownHit> hits)
{
    input.validate();
    output.validate();
    if (input.sampleRate != output.sampleRate || input.channels.size() != output.channels.size()
        || input.frames() != output.frames())
        throw std::invalid_argument("Report audio shapes differ");
    for (const auto& hit : hits)
    {
        if (hit.onsetSample < 0 || hit.lengthSamples <= 0
            || static_cast<std::uint64_t>(hit.onsetSample) > input.frames()
            || static_cast<std::uint64_t>(hit.lengthSamples)
                   > input.frames() - static_cast<std::size_t>(hit.onsetSample))
            throw std::invalid_argument("Known hit exceeds audio bounds");
    }

    stream.imbue(std::locale::classic());
    stream << std::setprecision(17);
    stream << "version,processor,kind,sample_rate,channel,start_sample,length_samples,"
              "expected_peak_dbfs,input_peak_dbfs,output_peak_dbfs,input_rms_dbfs,"
              "output_rms_dbfs,max_abs_error\n";
    for (std::size_t channel = 0; channel < input.channels.size(); ++channel)
    {
        const auto prefix = [&](const char* kind, std::size_t start, std::size_t length)
        {
            stream << BEAT_LEVELER_VERSION << ",passthrough," << kind << ',' << input.sampleRate
                   << ',' << channel + 1 << ',' << start << ',' << length;
        };
        prefix("summary", 0, input.frames());
        stream << ',';
        writeRow(stream, input, output, channel, 0, input.frames());
        for (const auto& hit : hits)
        {
            const auto start = static_cast<std::size_t>(hit.onsetSample);
            const auto length = static_cast<std::size_t>(hit.lengthSamples);
            prefix("known_hit", start, length);
            stream << ',' << hit.peakDb + (channel == 0 ? 0.0 : db(0.5));
            writeRow(stream, input, output, channel, start, length);
        }
    }
    if (!stream)
        throw std::runtime_error("Failed to write report");
}

} // namespace beat::leveler::harness
