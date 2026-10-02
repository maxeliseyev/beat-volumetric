#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

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
    windowParameter = parameters.getRawParameterValue("window_ms");
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
    result.push_back(std::make_unique<juce::AudioParameterFloat>("window_ms", "Window",
        juce::NormalisableRange<float>(20.0f, 400.0f, 1.0f), 120.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value)) + " ms"; })
            .withValueFromStringFunction([](const juce::String& text)
            {
                return juce::jlimit(20.0f, 400.0f, text.getFloatValue());
            })));
    result.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, percent));
    return { result.begin(), result.end() };
}

void BeatVolumetricAudioProcessor::prepareToPlay(double sampleRate, int)
{
    leveler.prepare(sampleRate, static_cast<std::size_t>(getTotalNumInputChannels()));
    leveler.reset();
    hitTelemetry.clear();
    setLatencySamples(static_cast<int>(leveler.latencySamples()));
}

void BeatVolumetricAudioProcessor::releaseResources()
{
    leveler.reset();
    hitTelemetry.clear();
}

bool BeatVolumetricAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    return input == layouts.getMainOutputChannelSet()
           && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void BeatVolumetricAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    const beat::leveler::LevelerParameters levelerParameters {
        strengthParameter != nullptr ? strengthParameter->load(std::memory_order_relaxed) : 0.5f,
        targetModeParameter == nullptr || targetModeParameter->load(std::memory_order_relaxed) < 0.5f,
        targetDbfsParameter != nullptr ? targetDbfsParameter->load(std::memory_order_relaxed) : -12.0f,
        mixParameter != nullptr ? mixParameter->load(std::memory_order_relaxed) : 1.0f,
        6.0f,
        12.0f,
        windowParameter != nullptr ? windowParameter->load(std::memory_order_relaxed) : 120.0f };
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
