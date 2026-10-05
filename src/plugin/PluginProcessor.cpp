#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstddef>

namespace
{
float db(float value)
{
    return 20.0f * std::log10(juce::jmax(value, 1.0e-12f));
}
}

BeatVolumetricAudioProcessor::BeatVolumetricAudioProcessor()
    : AudioProcessor(buses()), parameters(*this, nullptr, "BeatVolumetric", parameterLayout())
{
    strengthParameter = parameters.getRawParameterValue("strength");
    targetModeParameter = parameters.getRawParameterValue("target_mode");
    targetDbfsParameter = parameters.getRawParameterValue("target_dbfs");
    holdParameter = parameters.getRawParameterValue("hold_ms");
    releaseParameter = parameters.getRawParameterValue("release_ms");
    releaseCurveParameter = parameters.getRawParameterValue("release_curve");
    modeParameter = parameters.getRawParameterValue("mode");
    mixParameter = parameters.getRawParameterValue("mix");
}

juce::AudioProcessor::BusesProperties BeatVolumetricAudioProcessor::buses()
{
    return BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout BeatVolumetricAudioProcessor::parameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> result;
    const auto percent = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int)
        {
            return juce::String(juce::roundToInt(value * 100.0f)) + " %";
        })
        .withValueFromStringFunction([](const juce::String& text)
        {
            return juce::jlimit(0.0f, 1.0f, text.getFloatValue() / 100.0f);
        });
    result.push_back(std::make_unique<juce::AudioParameterFloat>("strength", "Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f, percent));
    result.push_back(std::make_unique<juce::AudioParameterChoice>("target_mode", "Target",
        juce::StringArray { "Auto", "Manual" }, 0));
    result.push_back(std::make_unique<juce::AudioParameterFloat>("target_dbfs", "Target level",
        juce::NormalisableRange<float>(-36.0f, -3.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " dB"; })
            .withValueFromStringFunction([](const juce::String& text)
            {
                return juce::jlimit(-36.0f, -3.0f, text.getFloatValue());
            })));
    const auto milliseconds = [](float low, float high, float initial, const juce::String& id,
                                 const juce::String& name)
    {
        return std::make_unique<juce::AudioParameterFloat>(id, name,
            juce::NormalisableRange<float>(low, high, 1.0f), initial,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value)) + " ms"; })
                .withValueFromStringFunction([low, high](const juce::String& text)
                {
                    return juce::jlimit(low, high, text.getFloatValue());
                }));
    };
    // "window_ms" is retired: its ID is not reused. Hold and Release replace it.
    result.push_back(milliseconds(20.0f, 400.0f, 120.0f, "hold_ms", "Hold"));
    result.push_back(milliseconds(1.0f, 200.0f, 8.0f, "release_ms", "Release"));
    result.push_back(std::make_unique<juce::AudioParameterChoice>("release_curve", "Release curve",
        juce::StringArray { "Linear", "Curved" }, 0));
    result.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode",
        juce::StringArray { "Both", "Cut loud", "Lift quiet" }, 0));
    result.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, percent));
    return { result.begin(), result.end() };
}

void BeatVolumetricAudioProcessor::prepareToPlay(double sampleRate, int)
{
    leveler.prepare(sampleRate, static_cast<std::size_t>(getTotalNumInputChannels()));
    leveler.reset();
    hitTelemetry.clear();
    hitScope.prepare(sampleRate, leveler.latencySamples());
    inputCounter = 0;
    setLatencySamples(static_cast<int>(leveler.latencySamples()));
}

void BeatVolumetricAudioProcessor::releaseResources()
{
    leveler.reset();
    hitTelemetry.clear();
    hitScope.reset();
    inputCounter = 0;
}

bool BeatVolumetricAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet()
           && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

namespace
{

beat::leveler::LevelingMode modeFor(float choice) noexcept
{
    if (choice > 1.5f)
        return beat::leveler::LevelingMode::liftQuiet;
    return choice > 0.5f ? beat::leveler::LevelingMode::cutLoud : beat::leveler::LevelingMode::both;
}

} // namespace

void BeatVolumetricAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    const auto sampleCount = buffer.getNumSamples();
    const auto inputChannels = getTotalNumInputChannels();
    if (inputChannels > 0 && sampleCount > 0)
    {
        hitScope.noteDry(buffer.getReadPointer(0),
                         inputChannels > 1 ? buffer.getReadPointer(1) : nullptr,
                         static_cast<std::size_t>(sampleCount),
                         inputCounter);
    }
    const beat::leveler::LevelerParameters levelerParameters {
        strengthParameter != nullptr ? strengthParameter->load(std::memory_order_relaxed) : 0.5f,
        targetModeParameter == nullptr || targetModeParameter->load(std::memory_order_relaxed) < 0.5f,
        targetDbfsParameter != nullptr ? targetDbfsParameter->load(std::memory_order_relaxed) : -12.0f,
        mixParameter != nullptr ? mixParameter->load(std::memory_order_relaxed) : 1.0f,
        6.0f,
        12.0f,
        holdParameter != nullptr ? holdParameter->load(std::memory_order_relaxed) : 120.0f,
        releaseParameter != nullptr ? releaseParameter->load(std::memory_order_relaxed) : 8.0f,
        releaseCurveParameter != nullptr && releaseCurveParameter->load(std::memory_order_relaxed) > 0.5f
            ? beat::leveler::ReleaseCurve::curved
            : beat::leveler::ReleaseCurve::linear,
        modeFor(modeParameter != nullptr ? modeParameter->load(std::memory_order_relaxed) : 0.0f) };
    events.fill({});
    const auto measurementsWritten = leveler.process(buffer.getArrayOfReadPointers(),
                                                      buffer.getArrayOfWritePointers(),
                                                      static_cast<std::size_t>(buffer.getNumSamples()),
                                                      levelerParameters, events, measurements);
    for (const auto& event : events)
    {
        if (event.id != 0)
        {
            hitCount.fetch_add(1, std::memory_order_relaxed);
            confidence.store(event.confidence, std::memory_order_relaxed);
            onsetSample.store(event.onsetSample, std::memory_order_relaxed);
        }
    }
    for (std::size_t index = 0; index < measurementsWritten; ++index)
        lastPeak.store(db(measurements[index].peak), std::memory_order_relaxed);
    for (const auto& decision : leveler.decisions())
        hitTelemetry.push(decision.onsetSample, decision.measuredDb, decision.gainDb, decision.targetDb);
    if (inputChannels > 0 && sampleCount > 0)
    {
        HitScope::Clock clock;
        if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition(); position.hasValue())
            {
                const auto bpm = position->getBpm();
                const auto ppq = position->getPpqPosition();
                if (bpm.hasValue() && ppq.hasValue() && *bpm > 1.0)
                {
                    clock.musical = true;
                    clock.playing = position->getIsPlaying();
                    clock.bpm = *bpm;
                    clock.ppq = *ppq;
                    if (const auto signature = position->getTimeSignature();
                        signature.hasValue() && signature->denominator > 0)
                        clock.quartersPerBar = static_cast<double>(signature->numerator) * 4.0
                                               / static_cast<double>(signature->denominator);
                }
            }
        }
        hitScope.noteWet(buffer.getReadPointer(0),
                         inputChannels > 1 ? buffer.getReadPointer(1) : nullptr,
                         static_cast<std::size_t>(sampleCount),
                         inputCounter,
                         clock);
        inputCounter += sampleCount;
    }
    gain.store(leveler.lastGainDb(), std::memory_order_relaxed);
    events.fill({});
}

juce::AudioProcessorEditor* BeatVolumetricAudioProcessor::createEditor()
{
    return new BeatVolumetricAudioProcessorEditor(*this);
}

void BeatVolumetricAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    copyXmlToBinary(*parameters.copyState().createXml(), destination);
}

void BeatVolumetricAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size); xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatVolumetricAudioProcessor();
}
