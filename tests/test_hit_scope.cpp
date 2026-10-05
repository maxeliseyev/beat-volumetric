#include "HitScope.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

void feed(HitScope& scope,
          const std::vector<float>& dry,
          const std::vector<float>& wet,
          std::int64_t start,
          const HitScope::Clock& clock,
          std::size_t block)
{
    std::size_t offset = 0;
    while (offset < dry.size())
    {
        const auto count = std::min(block, dry.size() - offset);
        const auto position = start + static_cast<std::int64_t>(offset);
        auto blockClock = clock;
        if (clock.musical)
        {
            blockClock.ppq = clock.ppq
                             + static_cast<double>(position - start) * clock.bpm / (60.0 * 48000.0);
        }
        scope.noteDry(dry.data() + offset, nullptr, count, position);
        scope.noteWet(wet.data() + offset, nullptr, count, position, blockClock);
        offset += count;
    }
}

int columnAt(double phase)
{
    return std::clamp(static_cast<int>(phase * static_cast<double>(HitScope::columns)),
                      0,
                      static_cast<int>(HitScope::columns) - 1);
}

} // namespace

TEST_CASE("The scope keeps two seconds of input and output in place", "[scope]")
{
    constexpr double rate = 48000.0;
    constexpr std::int64_t at = 24000;
    std::vector<float> dry(static_cast<std::size_t>(rate), 0.0f);
    std::vector<float> wet(dry.size(), 0.0f);
    dry[static_cast<std::size_t>(at)] = 0.5f;
    wet[static_cast<std::size_t>(at)] = -0.25f;

    HitScope scope;
    scope.prepare(rate, 0);
    feed(scope, dry, wet, 0, {}, 256);

    HitScope::Frame frame;
    REQUIRE(scope.copyFrame(frame));
    const auto column = columnAt(static_cast<double>(at) / (2.0 * rate));
    CHECK(frame.input[static_cast<std::size_t>(column)].maximum == Catch::Approx(0.5f));
    CHECK(frame.output[static_cast<std::size_t>(column)].minimum == Catch::Approx(-0.25f));
    CHECK_FALSE(frame.musical);

    dry[static_cast<std::size_t>(at)] = 0.1f;
    wet[static_cast<std::size_t>(at)] = 0.1f;
    feed(scope, dry, wet, static_cast<std::int64_t>(2.0 * rate), {}, 512);
    REQUIRE(scope.copyFrame(frame));
    CHECK(frame.input[static_cast<std::size_t>(column)].maximum == Catch::Approx(0.1f));
}

TEST_CASE("A playing host locks the same picture to two bars", "[scope]")
{
    constexpr double rate = 48000.0;
    HitScope::Clock clock;
    clock.musical = true;
    clock.playing = true;
    clock.bpm = 120.0;
    clock.quartersPerBar = 4.0;
    const auto samplesPerQuarter = static_cast<std::int64_t>(rate * 60.0 / clock.bpm);
    const auto at = samplesPerQuarter;

    std::vector<float> dry(static_cast<std::size_t>(samplesPerQuarter * 2), 0.0f);
    std::vector<float> wet(dry.size(), 0.0f);
    dry[static_cast<std::size_t>(at)] = 0.8f;
    wet[static_cast<std::size_t>(at)] = 0.4f;

    HitScope scope;
    scope.prepare(rate, 0);
    feed(scope, dry, wet, 0, clock, 128);

    HitScope::Frame frame;
    REQUIRE(scope.copyFrame(frame));
    CHECK(frame.musical);
    CHECK(frame.spanSeconds == Catch::Approx(4.0f));
    const auto column = columnAt(1.0 / (static_cast<double>(HitScope::bars) * clock.quartersPerBar));
    CHECK(frame.input[static_cast<std::size_t>(column)].maximum == Catch::Approx(0.8f));
    CHECK(frame.output[static_cast<std::size_t>(column)].maximum == Catch::Approx(0.4f));

    clock.playing = false;
    std::vector<float> silence(256, 1.0f);
    feed(scope, silence, silence, samplesPerQuarter * 2, clock, silence.size());
    REQUIRE(scope.copyFrame(frame));
    CHECK(frame.input[static_cast<std::size_t>(column)].maximum == Catch::Approx(0.8f));

    clock.playing = true;
    clock.ppq = 32.0;
    feed(scope, silence, silence, samplesPerQuarter * 2, clock, silence.size());
    REQUIRE(scope.copyFrame(frame));
    CHECK(frame.input[static_cast<std::size_t>(column)].maximum == Catch::Approx(0.0f).margin(0.001f));
}
