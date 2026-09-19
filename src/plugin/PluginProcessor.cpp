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
    result.push_back(std::make_unique<juce::AudioParameterFloat>("strength", "Strength",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    result.push_back(std::make_unique<juce::AudioParameterChoice>("target_mode", "Target",
        juce::StringArray { "Auto", "Manual" }, 0));
    result.push_back(std::make_unique<juce::AudioParameterFloat>("target_dbfs", "Target level",
        juce::NormalisableRange<float>(-36.0f, -3.0f, 0.1f), -12.0f));
    result.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    return { result.begin(), result.end() };
}

void BeatVolumetricAudioProcessor::prepareToPlay(double sampleRate, int)
{
    leveler.prepare(sampleRate, static_cast<std::size_t>(getTotalNumInputChannels()));
    leveler.reset();
    scope.prepare(static_cast<std::size_t>(sampleRate));
    setLatencySamples(static_cast<int>(leveler.latencySamples()));
}

void BeatVolumetricAudioProcessor::releaseResources()
{
    leveler.reset();
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
    const auto channels = static_cast<std::size_t>(getTotalNumInputChannels());
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    std::array<float, 2> scopeSamples {};
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (std::size_t channel = 0; channel < channels; ++channel)
            scopeSamples[channel] = buffer.getReadPointer(static_cast<int>(channel))[sample];
        scope.push(scopeSamples.data(), channels);
    }
    const beat::leveler::LevelerParameters levelerParameters {
        strengthParameter != nullptr ? strengthParameter->load(std::memory_order_relaxed) : 0.5f,
        targetModeParameter == nullptr || targetModeParameter->load(std::memory_order_relaxed) < 0.5f,
        targetDbfsParameter != nullptr ? targetDbfsParameter->load(std::memory_order_relaxed) : -12.0f,
        mixParameter != nullptr ? mixParameter->load(std::memory_order_relaxed) : 1.0f,
        6.0f,
        12.0f };
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
    gain.store(leveler.lastGainDb(), std::memory_order_relaxed);
    events.fill({});
    juce::ignoreUnused(channels);
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
