#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class ScopeRing
{
public:
    void prepare(std::size_t samples)
    {
        samples = std::max<std::size_t>(samples, 64);
        for (auto& channel : data) channel.assign(samples, 0.0f);
        write.store(0, std::memory_order_relaxed);
        samplesWritten.store(0, std::memory_order_relaxed);
    }
    void push(const float* samples, std::size_t channels) noexcept
    {
        if (samples == nullptr || channels == 0 || data.front().empty()) return;
        const auto index = write.load(std::memory_order_relaxed);
        for (std::size_t channel = 0; channel < std::min(channels, data.size()); ++channel) data[channel][index] = samples[channel];
        write.store((index + 1) % data.front().size(), std::memory_order_release);
        samplesWritten.fetch_add(1, std::memory_order_release);
    }
    void copyLast(float* destination, std::size_t count) const noexcept
    {
        if (destination == nullptr || count == 0 || data.front().empty()) return;
        count = std::min(count, data.front().size());
        auto index = (write.load(std::memory_order_acquire) + data.front().size() - count) % data.front().size();
        for (std::size_t item = 0; item < count; ++item) { destination[item] = data[0][index]; index = (index + 1) % data.front().size(); }
    }
    std::size_t length() const noexcept { return data.front().size(); }
    std::int64_t currentSample() const noexcept { return samplesWritten.load(std::memory_order_acquire); }
private:
    std::array<std::vector<float>, 2> data;
    std::atomic<std::size_t> write { 0 };
    std::atomic<std::int64_t> samplesWritten { 0 };
};
