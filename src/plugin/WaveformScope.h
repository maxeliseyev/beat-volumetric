#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

class WaveformScope final : public juce::Component
{
public:
    struct Hit
    {
        float position = -1.0f;
        float measuredDb = -240.0f;
        float resultDb = -240.0f;
    };

    void setWaveform(const float* samples, std::size_t count);
    void setHits(const Hit* hits, std::size_t count);
    void paint(juce::Graphics&) override;

private:
    std::vector<float> waveform;
    std::vector<Hit> hits;
};
