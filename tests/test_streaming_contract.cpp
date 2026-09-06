#include "dsp/DelayLine.h"
#include "dsp/Event.h"
#include "dsp/Latency.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace beat::leveler;

namespace
{

std::vector<float> delayed(const std::vector<float>& input, std::size_t amount)
{
    std::vector<float> expected(input.size(), 0.0f);
    if (amount < input.size())
        std::copy(input.begin(), input.end() - static_cast<std::ptrdiff_t>(amount),
                  expected.begin() + static_cast<std::ptrdiff_t>(amount));
    return expected;
}

std::vector<float> render(const std::vector<float>& input,
                          std::size_t delay,
                          const std::vector<std::size_t>& blocks)
{
    DelayLine line;
    line.prepare(1, delay + 2);
    line.setDelay(delay);
    std::vector<float> output(input.size(), 0.0f);
    std::size_t offset = 0, block = 0;
    while (offset < input.size())
    {
        const auto count = std::min(blocks[block], input.size() - offset);
        const float* source[] { input.data() + offset };
        float* destination[] { output.data() + offset };
        line.process(source, destination, count);
        offset += count;
        block = (block + 1) % blocks.size();
    }
    return output;
}

} // namespace

TEST_CASE("DelayLine emits an impulse after exactly the declared latency")
{
    constexpr std::size_t delay = 7;
    std::vector<float> input(64, 0.0f);
    input[0] = 1.0f;
    DelayLine line;
    line.prepare(1, delay + 1);
    line.setDelay(delay);
    std::vector<float> output(input.size());
    const float* source[] { input.data() };
    float* destination[] { output.data() };
    line.process(source, destination, input.size());
    REQUIRE(output == delayed(input, delay));
}

TEST_CASE("DelayLine is independent of host block boundaries and supports stereo")
{
    std::mt19937 random(17);
    std::vector<float> input(4097);
    for (auto& sample : input)
        sample = static_cast<float>(random()) / static_cast<float>(random.max());
    const auto expected = delayed(input, 13);
    REQUIRE(render(input, 13, { 1 }) == expected);
    REQUIRE(render(input, 13, { 127, 1, 511, 64 }) == expected);

    DelayLine line;
    line.prepare(2, 20);
    line.setDelay(13);
    std::vector<float> right(input.size());
    std::ranges::transform(input, right.begin(), [](float sample) { return -0.5f * sample; });
    std::vector<float> outputLeft(input.size()), outputRight(input.size());
    const float* source[] { input.data(), right.data() };
    float* destination[] { outputLeft.data(), outputRight.data() };
    line.process(source, destination, input.size());
    REQUIRE(outputLeft == expected);
    REQUIRE(outputRight == delayed(right, 13));
}

TEST_CASE("DelayLine reset clears history and rejects unprepared delays")
{
    DelayLine line;
    REQUIRE_THROWS(line.setDelay(0));
    REQUIRE_THROWS(line.prepare(0, 2));
    REQUIRE_THROWS(line.prepare(3, 2));
    line.prepare(1, 4);
    REQUIRE_THROWS(line.setDelay(5));
    line.setDelay(2);
    const std::array input { 1.0f, 0.0f, 0.0f };
    std::array output { 0.0f, 0.0f, 0.0f };
    const float* source[] { input.data() };
    float* destination[] { output.data() };
    line.process(source, destination, input.size());
    line.reset();
    line.process(source, destination, input.size());
    REQUIRE(output == std::array<float, 3> { 0.0f, 0.0f, 1.0f });
}

TEST_CASE("Latency budget exposes the full confirmation and measurement horizon")
{
    LatencyBudget budget { 4.0, 50.0, 2.0, 48000.0 };
    REQUIRE(budget.requiredSamples() == 2688);
    budget.sampleRate = 44100.0;
    REQUIRE(budget.requiredSamples() == 2470);
    budget.measurementMs = 0.0;
    REQUIRE(budget.requiredSamples() == 265);
    budget.confirmationMs = -1.0;
    REQUIRE_THROWS(budget.requiredSamples());
}

TEST_CASE("An event is late when its decision misses the delayed attack deadline")
{
    OnsetEvent event { 2, 9, 1000, 1961, 0.9f, EventProvenance::detector };
    REQUIRE(isReadyBeforeAttack(event, 1960, 1000));
    REQUIRE_FALSE(isReadyBeforeAttack(event, 2001, 1000));
    event.decisionReadySample = 2001;
    REQUIRE_FALSE(isReadyBeforeAttack(event, 1960, 1000));
}
