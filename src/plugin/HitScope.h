#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// Two bars of input and output, painted in place like a scroll-locked
// oscilloscope. The audio thread only writes preallocated columns. A torn
// read can drop a picture and does not affect audio.
class HitScope
{
public:
    static constexpr std::size_t columns = 512;
    static constexpr int bars = 2;

    struct Column
    {
        float minimum = 0.0f;
        float maximum = 0.0f;
        bool touched = false;
    };

    struct Clock
    {
        bool musical = false;
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;
        double quartersPerBar = 4.0;
    };

    struct Frame
    {
        std::array<Column, columns> input {};
        std::array<Column, columns> output {};
        int writeColumn = -1;
        bool musical = false;
        bool hasSignal = false;
        float spanSeconds = 2.0f;
    };

    void prepare(double sampleRate, std::size_t latencySamples)
    {
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        latency = static_cast<std::int64_t>(latencySamples);
        ring.assign(nextPowerOfTwo(latencySamples + 65536u), 0.0f);
        mask = ring.size() - 1;
        reset();
    }

    void reset() noexcept
    {
        building = {};
        slots[0] = {};
        slots[1] = {};
        publishedIndex.store(-1, std::memory_order_relaxed);
        writeColumn = -1;
        haveClock = false;
        blockOrigin = 0;
        expectedPpq = 0.0;
        if (!ring.empty())
            std::fill(ring.begin(), ring.end(), 0.0f);
    }

    void noteDry(const float* left,
                 const float* right,
                 std::size_t numSamples,
                 std::int64_t inputSampleStart) noexcept
    {
        if (left == nullptr || ring.empty())
            return;
        for (std::size_t index = 0; index < numSamples; ++index)
        {
            const auto first = left[index];
            const auto second = right != nullptr ? right[index] : first;
            ring[static_cast<std::size_t>(inputSampleStart + static_cast<std::int64_t>(index)) & mask]
                = 0.5f * (first + second);
        }
    }

    void noteWet(const float* left,
                 const float* right,
                 std::size_t numSamples,
                 std::int64_t outputSampleStart,
                 const Clock& clock) noexcept
    {
        if (left == nullptr || ring.empty() || numSamples == 0)
            return;
        if (clock.musical && !clock.playing)
        {
            publish(clock);
            return;
        }

        followClock(outputSampleStart, numSamples, clock);
        for (std::size_t index = 0; index < numSamples; ++index)
        {
            const auto outputIndex = outputSampleStart + static_cast<std::int64_t>(index);
            const auto source = outputIndex - latency;
            const auto dry = source >= 0 ? ring[static_cast<std::size_t>(source) & mask] : 0.0f;
            const auto first = left[index];
            const auto second = right != nullptr ? right[index] : first;
            plot(columnFor(outputIndex, clock), dry, 0.5f * (first + second));
        }
        publish(clock);
    }

    bool copyFrame(Frame& destination) const noexcept
    {
        const auto index = publishedIndex.load(std::memory_order_acquire);
        if (index < 0)
            return false;
        destination = slots[static_cast<std::size_t>(index)];
        return true;
    }

private:
    static std::size_t nextPowerOfTwo(std::size_t value) noexcept
    {
        std::size_t power = 1;
        while (power < value)
            power <<= 1u;
        return power;
    }

    double windowQuarters(const Clock& clock) const noexcept
    {
        return static_cast<double>(bars) * std::max(1.0, clock.quartersPerBar);
    }

    int columnFor(std::int64_t outputIndex, const Clock& clock) const noexcept
    {
        double phase = 0.0;
        if (clock.musical && clock.bpm > 1.0)
        {
            const auto window = windowQuarters(clock);
            const auto ppq = clock.ppq
                             + static_cast<double>(outputIndex - blockOrigin) * clock.bpm / (60.0 * rate);
            auto wrapped = std::fmod(ppq, window);
            if (wrapped < 0.0)
                wrapped += window;
            phase = window > 0.0 ? wrapped / window : 0.0;
        }
        else
        {
            const auto windowSamples = 2.0 * rate;
            auto wrapped = std::fmod(static_cast<double>(outputIndex), windowSamples);
            if (wrapped < 0.0)
                wrapped += windowSamples;
            phase = wrapped / windowSamples;
        }
        const auto column = static_cast<int>(phase * static_cast<double>(columns));
        return std::clamp(column, 0, static_cast<int>(columns) - 1);
    }

    void followClock(std::int64_t outputSampleStart, std::size_t numSamples, const Clock& clock) noexcept
    {
        if (clock.musical && haveClock && lastMusical)
        {
            if (std::abs(clock.ppq - expectedPpq) > 1.0)
                clearColumns();
        }
        else if (haveClock && clock.musical != lastMusical)
        {
            clearColumns();
        }
        blockOrigin = outputSampleStart;
        expectedPpq = clock.ppq + static_cast<double>(numSamples) * clock.bpm / (60.0 * rate);
        lastMusical = clock.musical;
        haveClock = true;
    }

    void clearColumns() noexcept
    {
        building.input.fill({});
        building.output.fill({});
        building.hasSignal = false;
        writeColumn = -1;
    }

    void resetColumn(int column) noexcept
    {
        building.input[static_cast<std::size_t>(column)] = {};
        building.output[static_cast<std::size_t>(column)] = {};
    }

    void plot(int column, float dry, float wet) noexcept
    {
        if (column != writeColumn)
        {
            if (writeColumn < 0)
            {
                resetColumn(column);
            }
            else
            {
                auto distance = column - writeColumn;
                if (distance < 0)
                    distance += static_cast<int>(columns);
                if (distance > 64)
                    clearColumns();
                else
                    for (int step = 1; step <= distance; ++step)
                        resetColumn((writeColumn + step) % static_cast<int>(columns));
            }
            writeColumn = column;
        }

        auto accumulate = [](Column& slot, float sample)
        {
            if (!slot.touched)
            {
                slot.minimum = slot.maximum = sample;
                slot.touched = true;
            }
            else
            {
                slot.minimum = std::min(slot.minimum, sample);
                slot.maximum = std::max(slot.maximum, sample);
            }
        };
        accumulate(building.input[static_cast<std::size_t>(column)], dry);
        accumulate(building.output[static_cast<std::size_t>(column)], wet);
        building.hasSignal = true;
        building.writeColumn = column;
    }

    void publish(const Clock& clock) noexcept
    {
        building.musical = clock.musical;
        building.spanSeconds = clock.musical && clock.bpm > 1.0
                                   ? static_cast<float>(windowQuarters(clock) * 60.0 / clock.bpm)
                                   : 2.0f;
        const auto current = publishedIndex.load(std::memory_order_relaxed);
        const auto write = current == 0 ? 1 : 0;
        slots[static_cast<std::size_t>(write)] = building;
        publishedIndex.store(write, std::memory_order_release);
    }

    double rate = 48000.0;
    std::int64_t latency = 0;
    std::vector<float> ring;
    std::size_t mask = 0;
    int writeColumn = -1;
    bool haveClock = false;
    bool lastMusical = false;
    std::int64_t blockOrigin = 0;
    double expectedPpq = 0.0;
    Frame building {};
    std::array<Frame, 2> slots {};
    std::atomic<int> publishedIndex { -1 };
};
