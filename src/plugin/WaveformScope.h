#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
class WaveformScope final : public juce::Component
{
public:
    void setWaveform(const float*, std::size_t);
    void setMarker(float position, float confidence);
    void paint(juce::Graphics&) override;
private:
    std::vector<float> waveform;
    float markerPosition = -1.0f;
    float markerConfidence = 0.0f;
};
