#pragma once
// ============================================================================
// StudioFeel — Lock-Free Single-Producer Single-Consumer (SPSC) Queue
// ============================================================================
// Used to pass ParameterUpdateCommand objects from the UI/IPC thread to the
// real-time audio thread in APOProcess(). The producer (UI) enqueues parameter
// changes; the consumer (audio thread) drains the queue at the start of each
// audio buffer without blocking.
//
// This is a bounded ring buffer implementation — no dynamic allocation.
// If the queue is full, Enqueue returns false and the command is dropped.
// The audio thread will pick up the next update; dropped intermediate
// values are acceptable for continuous parameters like gain sliders.
// ============================================================================

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <new>

namespace StudioFeel {

template <typename T, uint32_t Capacity = 256>
class LockFreeQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    LockFreeQueue()
        : m_head(0)
        , m_tail(0)
    {
        // Zero-initialize the buffer
        for (uint32_t i = 0; i < Capacity; ++i) {
            new (&m_buffer[i]) T{};
        }
    }

    ~LockFreeQueue() {
        for (uint32_t i = 0; i < Capacity; ++i) {
            reinterpret_cast<T*>(&m_buffer[i])->~T();
        }
    }

    // Non-copyable, non-movable
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // ========================================================================
    // Producer side — call from UI / IPC thread only
    // ========================================================================

    // Enqueue a value. Returns true on success, false if queue is full.
    // Thread-safe for a single producer.
    bool Enqueue(const T& value) {
        const uint32_t currentTail = m_tail.load(std::memory_order_relaxed);
        const uint32_t nextTail = (currentTail + 1) & kMask;

        // Check if full (tail would catch up to head)
        if (nextTail == m_head.load(std::memory_order_acquire)) {
            return false;  // Queue full — drop this update
        }

        // Write the value into the slot
        *reinterpret_cast<T*>(&m_buffer[currentTail]) = value;

        // Publish the new tail (makes the value visible to consumer)
        m_tail.store(nextTail, std::memory_order_release);
        return true;
    }

    // Move-enqueue variant for non-trivially-copyable types
    bool Enqueue(T&& value) {
        const uint32_t currentTail = m_tail.load(std::memory_order_relaxed);
        const uint32_t nextTail = (currentTail + 1) & kMask;

        if (nextTail == m_head.load(std::memory_order_acquire)) {
            return false;
        }

        *reinterpret_cast<T*>(&m_buffer[currentTail]) = std::move(value);
        m_tail.store(nextTail, std::memory_order_release);
        return true;
    }

    // ========================================================================
    // Consumer side — call from real-time audio thread only
    // ========================================================================

    // Dequeue a value. Returns true if a value was available, false if empty.
    // Thread-safe for a single consumer. Does NOT allocate or block.
    bool Dequeue(T& outValue) {
        const uint32_t currentHead = m_head.load(std::memory_order_relaxed);

        // Check if empty
        if (currentHead == m_tail.load(std::memory_order_acquire)) {
            return false;  // Queue empty
        }

        // Read the value
        outValue = *reinterpret_cast<const T*>(&m_buffer[currentHead]);

        // Advance head (frees the slot for producer)
        m_head.store((currentHead + 1) & kMask, std::memory_order_release);
        return true;
    }

    // ========================================================================
    // Informational (can be called from any thread, but values are approximate)
    // ========================================================================

    bool IsEmpty() const {
        return m_head.load(std::memory_order_acquire) ==
               m_tail.load(std::memory_order_acquire);
    }

    uint32_t Size() const {
        const uint32_t head = m_head.load(std::memory_order_acquire);
        const uint32_t tail = m_tail.load(std::memory_order_acquire);
        return (tail - head) & kMask;
    }

    static constexpr uint32_t GetCapacity() { return Capacity; }

private:
    static constexpr uint32_t kMask = Capacity - 1;

    // Separate cache lines for head and tail to avoid false sharing.
    // Producer writes m_tail; consumer writes m_head.
    alignas(64) std::atomic<uint32_t> m_head;
    alignas(64) std::atomic<uint32_t> m_tail;

    // Storage for queue elements — aligned for cache friendliness
    alignas(alignof(T)) typename std::aligned_storage<sizeof(T), alignof(T)>::type m_buffer[Capacity];
};

} // namespace StudioFeel
