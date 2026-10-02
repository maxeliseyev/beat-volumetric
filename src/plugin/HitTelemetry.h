#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Bounded hit decisions for the editor. The audio thread overwrites the oldest
// slot; a torn read can drop or mix one visual point and does not affect audio.
struct HitTelemetrySample
{
    std::int64_t onsetSample = -1;
    float measuredDb = -240.0f;
    float gainDb = 0.0f;
    float targetDb = -240.0f;
};

class HitTelemetry
{
public:
    void clear() noexcept
    {
        for (auto& slot : slots)
        {
            slot.measuredBits.store(0, std::memory_order_relaxed);
            slot.gainBits.store(0, std::memory_order_relaxed);
            slot.targetBits.store(0, std::memory_order_relaxed);
            slot.onset.store(-1, std::memory_order_relaxed);
        }
        write.store(0, std::memory_order_relaxed);
    }

    void push(std::int64_t onsetSample, float measuredDb, float gainDb, float targetDb) noexcept
    {
        const auto index = write.fetch_add(1, std::memory_order_relaxed) % slots.size();
        auto& slot = slots[index];
        slot.measuredBits.store(bits(measuredDb), std::memory_order_relaxed);
        slot.gainBits.store(bits(gainDb), std::memory_order_relaxed);
        slot.targetBits.store(bits(targetDb), std::memory_order_relaxed);
        slot.onset.store(onsetSample, std::memory_order_release);
    }

    std::size_t copy(HitTelemetrySample* destination, std::size_t capacity) const noexcept
    {
        if (destination == nullptr || capacity == 0)
            return 0;

        std::size_t written = 0;
        for (const auto& slot : slots)
        {
            const auto onsetSample = slot.onset.load(std::memory_order_acquire);
            if (onsetSample < 0)
                continue;
            destination[written++] = {
                onsetSample,
                number(slot.measuredBits.load(std::memory_order_relaxed)),
                number(slot.gainBits.load(std::memory_order_relaxed)),
                number(slot.targetBits.load(std::memory_order_relaxed))
            };
            if (written == capacity)
                break;
        }
        return written;
    }

private:
    struct Slot
    {
        std::atomic<std::int64_t> onset { -1 };
        std::atomic<std::uint32_t> measuredBits { 0 };
        std::atomic<std::uint32_t> gainBits { 0 };
        std::atomic<std::uint32_t> targetBits { 0 };
    };

    static std::uint32_t bits(float value) noexcept
    {
        std::uint32_t result = 0;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    static float number(std::uint32_t value) noexcept
    {
        float result = 0.0f;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    std::array<Slot, 16> slots {};
    std::atomic<std::uint32_t> write { 0 };
};
