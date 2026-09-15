#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
class WaveformScope final : public juce::Component { public: void setWaveform(const float*, std::size_t); void paint(juce::Graphics&) override; private: std::vector<float> waveform; };
