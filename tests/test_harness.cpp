#include "Harness.h"
#include "dsp/PassthroughProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>

using namespace beat::leveler;
using namespace beat::leveler::harness;

TEST_CASE("Passthrough supports empty, disjoint and in-place stereo buffers")
{
    PassthroughProcessor processor;
    processor.process(nullptr, nullptr, 2, 0);
    std::array left { 0.0f, -0.0f, 0.25f, -0.8f, 1.5f };
    std::array right { -0.5f, 0.1f, -1.2f, 0.0f, 0.75f };
    std::array<float, 7> outputLeft {}, outputRight {};
    outputLeft.fill(9.0f);
    outputRight.fill(9.0f);
    const float* input[] { left.data(), right.data() };
    float* output[] { outputLeft.data() + 1, outputRight.data() + 1 };
    processor.process(input, output, 2, left.size());
    REQUIRE(std::equal(left.begin(), left.end(), outputLeft.begin() + 1));
    REQUIRE(std::equal(right.begin(), right.end(), outputRight.begin() + 1));
    REQUIRE(std::signbit(outputLeft[2]));
    REQUIRE(outputLeft.front() == 9.0f);
    REQUIRE(outputLeft.back() == 9.0f);
    REQUIRE(outputRight.front() == 9.0f);
    REQUIRE(outputRight.back() == 9.0f);
    const auto originalLeft = left;
    const auto originalRight = right;
    float* inPlace[] { left.data(), right.data() };
    processor.process(input, inPlace, 2, left.size());
    REQUIRE(left == originalLeft);
    REQUIRE(right == originalRight);
}

TEST_CASE("Synthetic fixture has known onsets, peaks, silence and deterministic samples")
{
    for (const int rate : { 44100, 48000, 88200, 96000, 192000 })
    {
        const auto fixture = makeSynthetic(rate, 2);
        REQUIRE(fixture.audio.channels == makeSynthetic(rate, 2).audio.channels);
        REQUIRE(fixture.audio.frames() == static_cast<std::size_t>(rate) * 2);
        REQUIRE(fixture.hits.size() == 4);
        for (const auto& hit : fixture.hits)
        {
            const auto start = static_cast<std::size_t>(hit.onsetSample);
            REQUIRE(fixture.audio.channels[0][start - 1] == 0.0f);
            const auto amplitude = std::pow(10.0, hit.peakDb / 20.0);
            REQUIRE(fixture.audio.channels[0][start] == Catch::Approx(amplitude).margin(1.0e-7));
            double peak = 0.0;
            for (std::size_t i = start; i < start + static_cast<std::size_t>(hit.lengthSamples); ++i)
            {
                peak = std::max(peak, std::abs(static_cast<double>(fixture.audio.channels[0][i])));
                REQUIRE(fixture.audio.channels[1][i] == -0.5f * fixture.audio.channels[0][i]);
            }
            REQUIRE(20.0 * std::log10(peak) == Catch::Approx(hit.peakDb).margin(1.0e-5));
        }
        REQUIRE(fixture.audio.channels[0].back() == 0.0f);
    }
}

TEST_CASE("Rendering preserves every sample for fixed and changing block patterns")
{
    std::mt19937 random(42);
    std::vector<std::size_t> changing { 127, 1, 511 };
    for (int i = 0; i < 64; ++i)
        changing.push_back(1 + random() % 2048);
    for (const int rate : { 44100, 48000, 88200, 96000, 192000 })
    {
        for (const std::size_t channels : { 1, 2 })
        {
            const auto fixture = makeSynthetic(rate, channels);
            for (const std::size_t size : { 1, 16, 32, 64, 127, 256, 511, 1024, 2048 })
            {
                const std::array blocks { size };
                const auto output = render(fixture.audio, blocks);
                REQUIRE(output.sampleRate == rate);
                REQUIRE(output.channels == fixture.audio.channels);
            }
            REQUIRE(render(fixture.audio, changing).channels == fixture.audio.channels);
        }
    }
}

TEST_CASE("Harness rejects invalid input and handles empty audio and silence")
{
    const std::array<std::size_t, 1> blocks { 127 };
    Audio audio { 48000, { {} } };
    REQUIRE(render(audio, blocks).frames() == 0);
    REQUIRE_THROWS(render(audio, {}));
    const std::array<std::size_t, 2> invalidBlocks { 1, 0 };
    REQUIRE_THROWS(render(audio, invalidBlocks));
    REQUIRE_THROWS(makeSynthetic(0, 1));
    REQUIRE_THROWS(makeSynthetic(48000, 3));
    audio.channels = { { 0.0f, 0.0f } };
    std::ostringstream report;
    writeReport(report, audio, render(audio, blocks), {});
    REQUIRE(report.str().find(",-240,-240,-240,-240,0\n") != std::string::npos);
    audio.channels[0][0] = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_THROWS(render(audio, blocks));
    audio.channels = { { 0.0f }, {} };
    REQUIRE_THROWS(render(audio, blocks));
}

TEST_CASE("Report measures changed output instead of inferring it from requested gain")
{
    Audio input { 48000, { { 1.0f, -1.0f } } };
    Audio output { 48000, { { 0.5f, -0.5f } } };
    std::ostringstream report;
    writeReport(report, input, output, {});
    std::istringstream rows(report.str());
    std::string header, row;
    std::getline(rows, header);
    std::getline(rows, row);
    std::istringstream fields(row);
    std::vector<std::string> values;
    for (std::string value; std::getline(fields, value, ',');)
        values.push_back(value);
    REQUIRE(values.size() == 13);
    REQUIRE(std::stod(values[8]) == 0.0);
    REQUIRE(std::stod(values[9]) == Catch::Approx(-6.020599913));
    REQUIRE(std::stod(values[11]) == Catch::Approx(-6.020599913));
    REQUIRE(std::stod(values[12]) == 0.5);
    const std::array<KnownHit, 1> badHit { KnownHit { 1, 2, 0 } };
    REQUIRE_THROWS(writeReport(report, input, output, badHit));
}
