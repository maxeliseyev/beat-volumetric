#include "Harness.h"
#include "dsp/DelayLine.h"
#include "dsp/Event.h"
#include "dsp/Latency.h"
#include "dsp/StreamingLeveler.h"

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

struct LevelerRender
{
    std::vector<std::vector<float>> output;
    std::vector<OnsetEvent> events;
    std::vector<HitMeasurement> measurements;
};

LevelerRender renderLeveler(const std::vector<std::vector<float>>& input,
                            int sampleRate,
                            const LevelerParameters& parameters,
                            const std::vector<std::size_t>& blocks)
{
    LevelerRender result;
    result.output.assign(input.size(), std::vector<float>(input.front().size(), 0.0f));
    StreamingLeveler leveler;
    leveler.prepare(sampleRate, input.size());
    std::array<const float*, 2> source {};
    std::array<float*, 2> destination {};
    std::array<OnsetEvent, 32> events {};
    std::array<HitMeasurement, 32> measurements {};
    std::size_t offset = 0;
    std::size_t block = 0;
    while (offset < input.front().size())
    {
        const auto count = std::min(blocks[block], input.front().size() - offset);
        for (std::size_t channel = 0; channel < input.size(); ++channel)
        {
            source[channel] = input[channel].data() + offset;
            destination[channel] = result.output[channel].data() + offset;
        }
        const auto measurementCount = leveler.process(source.data(), destination.data(), count,
                                                      parameters, events, measurements);
        const auto eventCount = std::count_if(events.begin(), events.end(),
                                              [](const auto& event) { return event.id != 0; });
        result.events.insert(result.events.end(), events.begin(),
                             events.begin() + static_cast<std::ptrdiff_t>(eventCount));
        result.measurements.insert(result.measurements.end(), measurements.begin(),
                                   measurements.begin() + static_cast<std::ptrdiff_t>(measurementCount));
        events.fill({});
        offset += count;
        block = (block + 1) % blocks.size();
    }
    return result;
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

TEST_CASE("Streaming leveler keeps unity gain while applying the declared lookahead")
{
    const auto fixture = beat::leveler::harness::makeSynthetic(48000, 2);
    const LevelerParameters parameters { 0.0f, false, -12.0f, 1.0f, 6.0f, 12.0f };
    const auto rendered = renderLeveler(fixture.audio.channels, fixture.audio.sampleRate,
                                        parameters, { 127, 1, 511 });
    StreamingLeveler reference;
    reference.prepare(fixture.audio.sampleRate, fixture.audio.channels.size());
    const auto latency = reference.latencySamples();
    REQUIRE(rendered.events.size() == fixture.hits.size());
    REQUIRE(rendered.measurements.size() == fixture.hits.size());
    for (std::size_t channel = 0; channel < fixture.audio.channels.size(); ++channel)
    {
        for (std::size_t output = 0; output < rendered.output[channel].size(); ++output)
        {
            const auto expected = output >= latency ? fixture.audio.channels[channel][output - latency] : 0.0f;
            REQUIRE(rendered.output[channel][output] == expected);
        }
    }

    const LevelerParameters dryParameters { 1.0f, false, -12.0f, 0.0f, 6.0f, 12.0f };
    const auto dry = renderLeveler(fixture.audio.channels, fixture.audio.sampleRate,
                                   dryParameters, { 256 });
    REQUIRE(dry.output == rendered.output);
}

TEST_CASE("Streaming leveler applies one common measured gain independent of host blocks")
{
    const auto fixture = beat::leveler::harness::makeSynthetic(48000, 2);
    const LevelerParameters parameters { 0.5f, false, -12.0f, 1.0f, 6.0f, 12.0f };
    const auto single = renderLeveler(fixture.audio.channels, fixture.audio.sampleRate,
                                      parameters, { 1 });
    const auto varied = renderLeveler(fixture.audio.channels, fixture.audio.sampleRate,
                                      parameters, { 127, 1, 511 });
    REQUIRE(single.output == varied.output);
    REQUIRE(single.measurements.size() == fixture.hits.size());

    const auto& measurement = single.measurements.front();
    const auto measuredDb = 20.0f * std::log10(measurement.weightedRms);
    const auto requestedDb = std::clamp((parameters.targetDbfs - measuredDb) * parameters.strength,
                                        -parameters.maxCutDb, parameters.maxBoostDb);
    const auto expectedGain = std::pow(10.0f, requestedDb / 20.0f);
    const auto latency = static_cast<std::size_t>(std::llround(0.056 * fixture.audio.sampleRate));
    const auto source = static_cast<std::size_t>(fixture.hits.front().onsetSample) + 100;
    REQUIRE(std::abs(fixture.audio.channels[0][source]) > 1.0e-5f);
    REQUIRE(single.output[0][source + latency] / fixture.audio.channels[0][source]
            == Catch::Approx(expectedGain).margin(1.0e-5));
    REQUIRE(single.output[1][source + latency] / fixture.audio.channels[1][source]
            == Catch::Approx(expectedGain).margin(1.0e-5));
}

float plateauRatio(const std::vector<float>& input,
                   const std::vector<float>& output,
                   std::int64_t begin,
                   std::int64_t end,
                   std::size_t latency)
{
    auto best = static_cast<std::size_t>(begin);
    auto bestMagnitude = 0.0f;
    for (auto sample = begin; sample < end; ++sample)
    {
        const auto magnitude = std::abs(input[static_cast<std::size_t>(sample)]);
        if (magnitude > bestMagnitude)
        {
            bestMagnitude = magnitude;
            best = static_cast<std::size_t>(sample);
        }
    }
    REQUIRE(bestMagnitude > 1.0e-4f);
    return output[best + latency] / input[best];
}

TEST_CASE("Application window outlasts the measurement horizon and then releases")
{
    const auto fixture = beat::leveler::harness::makeSynthetic(48000, 1);
    const LevelerParameters held { 1.0f, false, -12.0f, 1.0f, 24.0f, 24.0f, 120.0f };
    const auto single = renderLeveler(fixture.audio.channels, fixture.audio.sampleRate, held, { 1 });
    const auto varied = renderLeveler(fixture.audio.channels, fixture.audio.sampleRate,
                                      held, { 127, 1, 511 });
    REQUIRE(single.output == varied.output);
    REQUIRE(single.measurements.size() == fixture.hits.size());

    const auto& measurement = single.measurements.front();
    const auto onset = measurement.event.onsetSample;
    const auto sampleRate = fixture.audio.sampleRate;
    const auto ms = [&](double milliseconds)
    {
        return static_cast<std::int64_t>(std::llround(milliseconds * 0.001 * sampleRate));
    };
    REQUIRE(std::abs(onset - fixture.hits.front().onsetSample) <= ms(1.0));
    REQUIRE(measurement.windowSamples < static_cast<std::size_t>(ms(35.0)));

    StreamingLeveler reference;
    reference.prepare(sampleRate, 1);
    const auto latency = reference.latencySamples();
    const auto measuredDb = 20.0f * std::log10(measurement.weightedRms);
    const auto requestedDb = std::clamp((held.targetDbfs - measuredDb) * held.strength,
                                        -held.maxCutDb, held.maxBoostDb);
    const auto expectedGain = std::pow(10.0f, requestedDb / 20.0f);
    REQUIRE(std::abs(requestedDb) > 1.0f);

    const auto pastMeasurement = plateauRatio(fixture.audio.channels[0], single.output[0],
                                              onset + ms(35.0), onset + ms(50.0), latency);
    REQUIRE(pastMeasurement == Catch::Approx(expectedGain).margin(1.0e-4));

    const LevelerParameters brief { 1.0f, false, -12.0f, 1.0f, 24.0f, 24.0f, 20.0f };
    const auto releasedEarly = renderLeveler(fixture.audio.channels, sampleRate, brief, { 64 });
    const auto afterBriefWindow = plateauRatio(fixture.audio.channels[0], releasedEarly.output[0],
                                               onset + ms(35.0), onset + ms(50.0), latency);
    REQUIRE(afterBriefWindow == Catch::Approx(1.0f).margin(1.0e-4));

    const LevelerParameters mid { 1.0f, false, -12.0f, 1.0f, 24.0f, 24.0f, 40.0f };
    const auto returns = renderLeveler(fixture.audio.channels, sampleRate, mid, { 64 });
    const auto onPlateau = plateauRatio(fixture.audio.channels[0], returns.output[0],
                                        onset + ms(10.0), onset + ms(25.0), latency);
    const auto afterReturn = plateauRatio(fixture.audio.channels[0], returns.output[0],
                                         onset + ms(55.0), onset + ms(68.0), latency);
    REQUIRE(onPlateau == Catch::Approx(expectedGain).margin(1.0e-4));
    REQUIRE(afterReturn == Catch::Approx(1.0f).margin(1.0e-4));
}

TEST_CASE("Release length and curve shape the return to unity")
{
    const auto fixture = beat::leveler::harness::makeSynthetic(48000, 1);
    const auto sampleRate = fixture.audio.sampleRate;
    const auto ms = [&](double milliseconds)
    {
        return static_cast<std::int64_t>(std::llround(milliseconds * 0.001 * sampleRate));
    };
    LevelerParameters linear { 1.0f, false, -12.0f, 1.0f, 24.0f, 24.0f, 40.0f, 40.0f,
                               ReleaseCurve::linear };
    auto curved = linear;
    curved.releaseCurve = ReleaseCurve::curved;

    const auto linearRender = renderLeveler(fixture.audio.channels, sampleRate, linear, { 64 });
    const auto curvedRender = renderLeveler(fixture.audio.channels, sampleRate, curved, { 64 });
    const auto varied = renderLeveler(fixture.audio.channels, sampleRate, curved, { 127, 1, 511 });
    REQUIRE(curvedRender.output == varied.output);

    const auto onset = linearRender.measurements.front().event.onsetSample;
    StreamingLeveler reference;
    reference.prepare(sampleRate, 1);
    const auto latency = reference.latencySamples();
    const auto measuredDb = 20.0f * std::log10(linearRender.measurements.front().weightedRms);
    const auto plateau = std::pow(10.0f, std::clamp((linear.targetDbfs - measuredDb) * linear.strength,
                                                     -linear.maxCutDb, linear.maxBoostDb) / 20.0f);
    REQUIRE(std::abs(plateau - 1.0f) > 0.1f);

    const auto at = [&](const LevelerRender& render, double fromMs)
    {
        return plateauRatio(fixture.audio.channels[0], render.output[0],
                            onset + ms(fromMs), onset + ms(fromMs + 1.0), latency);
    };
    // Release spans onset+40 ms .. onset+80 ms. A quarter in, the knee has barely left the plateau.
    REQUIRE(at(linearRender, 50.0) == Catch::Approx(1.0f + (plateau - 1.0f) * 0.75f).margin(0.03f * std::abs(plateau - 1.0f)));
    REQUIRE(at(curvedRender, 50.0) == Catch::Approx(1.0f + (plateau - 1.0f) * 0.8536f).margin(0.03f * std::abs(plateau - 1.0f)));
    REQUIRE(at(curvedRender, 70.0) == Catch::Approx(1.0f + (plateau - 1.0f) * 0.1464f).margin(0.03f * std::abs(plateau - 1.0f)));
}

TEST_CASE("Leveling mode restricts the direction of the gain")
{
    const auto fixture = beat::leveler::harness::makeSynthetic(48000, 1);
    std::vector<float> measured;
    const auto render = [&](LevelingMode mode, float target)
    {
        LevelerParameters parameters { 1.0f, false, target, 1.0f, 24.0f, 24.0f };
        parameters.mode = mode;
        StreamingLeveler leveler;
        leveler.prepare(fixture.audio.sampleRate, 1);
        std::vector<float> output(fixture.audio.channels[0].size());
        std::array<OnsetEvent, 32> events {};
        std::array<HitMeasurement, 32> measurements {};
        std::vector<float> gains;
        const float* source[] { fixture.audio.channels[0].data() };
        for (std::size_t offset = 0; offset < output.size(); offset += 256)
        {
            const auto count = std::min<std::size_t>(256, output.size() - offset);
            const float* in[] { source[0] + offset };
            float* out[] { output.data() + offset };
            leveler.process(in, out, count, parameters, events, measurements);
            for (const auto& decision : leveler.decisions())
            {
                gains.push_back(decision.gainDb);
                measured.push_back(decision.measuredDb);
            }
            events.fill({});
        }
        return gains;
    };
    render(LevelingMode::both, -12.0f);
    std::sort(measured.begin(), measured.end());
    // Aim at the middle of the measured hits so some are louder and some quieter.
    const auto target = measured[measured.size() / 2];
    const auto both = render(LevelingMode::both, target);
    const auto cut = render(LevelingMode::cutLoud, target);
    const auto lift = render(LevelingMode::liftQuiet, target);
    REQUIRE(both.size() == cut.size());
    REQUIRE(both.size() == lift.size());
    REQUIRE(std::any_of(both.begin(), both.end(), [](float gain) { return gain > 0.1f; }));
    REQUIRE(std::any_of(both.begin(), both.end(), [](float gain) { return gain < -0.1f; }));
    for (std::size_t index = 0; index < both.size(); ++index)
    {
        REQUIRE(cut[index] == Catch::Approx(std::min(both[index], 0.0f)).margin(1.0e-6));
        REQUIRE(lift[index] == Catch::Approx(std::max(both[index], 0.0f)).margin(1.0e-6));
    }
}
