#pragma once
// ============================================================================
// StudioFeel — DSP Engine
// ============================================================================
// Manages the complete EQ processing pipeline: a chain of biquad filters per
// channel, with lock-free parameter updates from the UI/IPC thread.
//
// Lifecycle:
//   1. Construct with channel count and sample rate
//   2. Call Initialize() with initial EQ configuration
//   3. Call ProcessAudio() from the real-time audio thread (APOProcess)
//   4. Call UpdateParameter() from the UI thread to change settings
//   5. ProcessAudio() drains the parameter queue at each buffer boundary
//
// Thread safety:
//   - ProcessAudio()      → real-time audio thread ONLY
//   - UpdateParameter()   → UI/IPC thread ONLY
//   - GetMagnitudeAt()    → any thread (reads atomic snapshot)
// ============================================================================

#include "BiquadFilter.h"
#include "Coefficients.h"
#include "LockFreeQueue.h"
#include "../../IPC/include/EQParameters.h"

#include <vector>
#include <array>
#include <atomic>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace StudioFeel {

class DSPEngine {
public:
    static constexpr uint32_t MAX_CHANNELS = 8;     // Up to 7.1 surround
    static constexpr uint32_t MAX_BANDS    = EQConfiguration::MAX_BANDS;

    // ========================================================================
    // Construction
    // ========================================================================

    DSPEngine()
        : m_numChannels(2)
        , m_sampleRate(48000)
        , m_masterEnabled(true)
        , m_masterGain(1.0f)
    {
        ResetAllFilters();
    }

    ~DSPEngine() = default;

    // Non-copyable (contains atomics and large filter arrays)
    DSPEngine(const DSPEngine&) = delete;
    DSPEngine& operator=(const DSPEngine&) = delete;

    // ========================================================================
    // Initialization (call before LockForProcess, non-real-time)
    // ========================================================================

    bool Initialize(uint32_t numChannels, uint32_t sampleRate,
                    const EQConfiguration& config)
    {
        if (numChannels == 0 || numChannels > MAX_CHANNELS) return false;
        if (sampleRate == 0) return false;

        m_numChannels = numChannels;
        m_sampleRate = sampleRate;
        m_currentConfig = config;
        m_currentConfig.Clamp();
        m_masterEnabled.store(config.masterEnabled, std::memory_order_relaxed);
        m_masterGain.store(DbToLinear(config.masterGainDb), std::memory_order_relaxed);

        RecalculateAllCoefficients();
        ResetAllFilters();
        return true;
    }

    // ========================================================================
    // Parameter Updates (call from UI/IPC thread)
    // ========================================================================

    bool UpdateParameter(const ParameterUpdateCommand& cmd) {
        return m_parameterQueue.Enqueue(cmd);
    }

    bool UpdateFullConfiguration(const EQConfiguration& config) {
        ParameterUpdateCommand cmd;
        cmd.commandType = ParameterUpdateCommand::Type::FullConfiguration;
        cmd.fullConfig = config;
        return m_parameterQueue.Enqueue(std::move(cmd));
    }

    // ========================================================================
    // Real-Time Audio Processing (call from audio thread ONLY)
    // ========================================================================

    // Process interleaved float32 audio in-place.
    // pData: pointer to interleaved samples (ch0_s0, ch1_s0, ch0_s1, ch1_s1, ...)
    // framesPerBuffer: number of audio frames (each frame = numChannels samples)
    void ProcessAudio(float* pData, uint32_t framesPerBuffer)
    {
        // 1. Drain pending parameter updates (lock-free)
        ProcessPendingUpdates();

        // 2. Early out if EQ is disabled
        if (!m_masterEnabled.load(std::memory_order_relaxed)) return;

        const uint32_t channels = m_numChannels;
        const float masterGain = m_masterGain.load(std::memory_order_relaxed);
        const uint32_t activeBands = m_activeBandCount;

        // 3. Process each band's filter chain across all channels
        for (uint32_t band = 0; band < activeBands; ++band) {
            if (!m_bandEnabled[band]) continue;

            for (uint32_t ch = 0; ch < channels; ++ch) {
                // Process this band for this channel on the interleaved buffer.
                // Stride = channels (samples for this channel are spaced by channel count).
                m_filters[ch][band].Process(
                    pData + ch,          // Start at this channel's first sample
                    framesPerBuffer,     // Number of frames
                    channels             // Stride between consecutive samples
                );
            }
        }

        // 4. Apply master gain if not unity
        if (std::abs(masterGain - 1.0f) > 0.0001f) {
            const uint32_t totalSamples = framesPerBuffer * channels;
            for (uint32_t i = 0; i < totalSamples; ++i) {
                pData[i] *= masterGain;
            }
        }
    }

    // ========================================================================
    // Frequency Response (for UI visualization, any thread)
    // ========================================================================

    // Get the composite magnitude response at a frequency (sum of all bands in dB)
    float GetCompositeMagnitudeAt(float frequencyHz) const {
        float totalDb = 0.0f;
        const uint32_t activeBands = m_activeBandCount;

        for (uint32_t band = 0; band < activeBands; ++band) {
            if (!m_bandEnabled[band]) continue;
            // Use channel 0's filter (all channels have identical coefficients)
            totalDb += m_filters[0][band].GetMagnitudeAt(frequencyHz,
                                                          static_cast<float>(m_sampleRate));
        }

        // Add master gain
        const float masterGain = m_masterGain.load(std::memory_order_relaxed);
        if (masterGain > 0.0f) {
            totalDb += 20.0f * std::log10(masterGain);
        }

        return totalDb;
    }

    // Get magnitude response for a single band
    float GetBandMagnitudeAt(uint32_t bandIndex, float frequencyHz) const {
        if (bandIndex >= m_activeBandCount) return 0.0f;
        return m_filters[0][bandIndex].GetMagnitudeAt(frequencyHz,
                                                       static_cast<float>(m_sampleRate));
    }

    // Fill an array with composite frequency response at log-spaced frequencies
    // outMagnitudes: array of at least 'numPoints' floats
    // numPoints: number of frequency points to evaluate
    // startHz: lowest frequency (default 20 Hz)
    // endHz: highest frequency (default 20000 Hz)
    void GetFrequencyResponse(float* outMagnitudes, uint32_t numPoints,
                               float startHz = 20.0f, float endHz = 20000.0f) const
    {
        const float logStart = std::log10(startHz);
        const float logEnd   = std::log10(endHz);
        const float logStep  = (logEnd - logStart) / static_cast<float>(numPoints - 1);

        for (uint32_t i = 0; i < numPoints; ++i) {
            const float freq = std::pow(10.0f, logStart + logStep * static_cast<float>(i));
            outMagnitudes[i] = GetCompositeMagnitudeAt(freq);
        }
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    uint32_t GetNumChannels() const { return m_numChannels; }
    uint32_t GetSampleRate() const { return m_sampleRate; }
    uint32_t GetActiveBandCount() const { return m_activeBandCount; }
    bool IsMasterEnabled() const { return m_masterEnabled.load(std::memory_order_relaxed); }

    const EQConfiguration& GetCurrentConfig() const { return m_currentConfig; }

private:
    // ========================================================================
    // Internal: Process parameter update queue (called at start of ProcessAudio)
    // ========================================================================

    void ProcessPendingUpdates() {
        ParameterUpdateCommand cmd;
        bool needRecalc = false;

        while (m_parameterQueue.Dequeue(cmd)) {
            switch (cmd.commandType) {
                case ParameterUpdateCommand::Type::SetBandGain:
                    if (cmd.bandIndex >= 0 && cmd.bandIndex < static_cast<int>(m_currentConfig.bands.size())) {
                        m_currentConfig.bands[cmd.bandIndex].gainDb = cmd.floatValue;
                        needRecalc = true;
                    }
                    break;

                case ParameterUpdateCommand::Type::SetBandFrequency:
                    if (cmd.bandIndex >= 0 && cmd.bandIndex < static_cast<int>(m_currentConfig.bands.size())) {
                        m_currentConfig.bands[cmd.bandIndex].frequency = cmd.floatValue;
                        needRecalc = true;
                    }
                    break;

                case ParameterUpdateCommand::Type::SetBandQ:
                    if (cmd.bandIndex >= 0 && cmd.bandIndex < static_cast<int>(m_currentConfig.bands.size())) {
                        m_currentConfig.bands[cmd.bandIndex].Q = cmd.floatValue;
                        needRecalc = true;
                    }
                    break;

                case ParameterUpdateCommand::Type::SetBandType:
                    if (cmd.bandIndex >= 0 && cmd.bandIndex < static_cast<int>(m_currentConfig.bands.size())) {
                        m_currentConfig.bands[cmd.bandIndex].type =
                            static_cast<FilterType>(cmd.uintValue);
                        needRecalc = true;
                    }
                    break;

                case ParameterUpdateCommand::Type::SetBandEnabled:
                    if (cmd.bandIndex >= 0 && cmd.bandIndex < static_cast<int>(m_currentConfig.bands.size())) {
                        m_currentConfig.bands[cmd.bandIndex].enabled = cmd.boolValue;
                        m_bandEnabled[cmd.bandIndex] = cmd.boolValue;
                    }
                    break;

                case ParameterUpdateCommand::Type::SetMasterGain:
                    m_currentConfig.masterGainDb = cmd.floatValue;
                    m_masterGain.store(DbToLinear(cmd.floatValue), std::memory_order_relaxed);
                    break;

                case ParameterUpdateCommand::Type::SetMasterEnabled:
                    m_currentConfig.masterEnabled = cmd.boolValue;
                    m_masterEnabled.store(cmd.boolValue, std::memory_order_relaxed);
                    break;

                case ParameterUpdateCommand::Type::SetSampleRate:
                    m_sampleRate = cmd.uintValue;
                    m_currentConfig.sampleRate = cmd.uintValue;
                    needRecalc = true;
                    break;

                case ParameterUpdateCommand::Type::FullConfiguration:
                    m_currentConfig = cmd.fullConfig;
                    m_currentConfig.Clamp();
                    m_masterEnabled.store(m_currentConfig.masterEnabled, std::memory_order_relaxed);
                    m_masterGain.store(DbToLinear(m_currentConfig.masterGainDb), std::memory_order_relaxed);
                    needRecalc = true;
                    break;
            }
        }

        if (needRecalc) {
            RecalculateAllCoefficients();
        }
    }

    // ========================================================================
    // Coefficient Recalculation
    // ========================================================================

    void RecalculateAllCoefficients() {
        const uint32_t bandCount = static_cast<uint32_t>(
            std::min(static_cast<size_t>(MAX_BANDS), m_currentConfig.bands.size()));
        m_activeBandCount = bandCount;

        for (uint32_t band = 0; band < bandCount; ++band) {
            const auto& bandConfig = m_currentConfig.bands[band];
            m_bandEnabled[band] = bandConfig.enabled;

            // Compute coefficients from band config
            BiquadCoefficients coeff = CoefficientCalculator::FromBandConfig(
                bandConfig, static_cast<float>(m_sampleRate));

            // Apply same coefficients to all channels
            for (uint32_t ch = 0; ch < m_numChannels; ++ch) {
                m_filters[ch][band].SetCoefficients(coeff);
            }
        }

        // Set remaining bands to identity
        for (uint32_t band = bandCount; band < MAX_BANDS; ++band) {
            m_bandEnabled[band] = false;
            BiquadCoefficients identity = BiquadCoefficients::Identity();
            for (uint32_t ch = 0; ch < MAX_CHANNELS; ++ch) {
                m_filters[ch][band].SetCoefficients(identity);
            }
        }
    }

    void ResetAllFilters() {
        for (uint32_t ch = 0; ch < MAX_CHANNELS; ++ch) {
            for (uint32_t band = 0; band < MAX_BANDS; ++band) {
                m_filters[ch][band].Reset();
            }
        }
    }

    static float DbToLinear(float db) {
        return std::pow(10.0f, db / 20.0f);
    }

    // ========================================================================
    // State
    // ========================================================================

    uint32_t m_numChannels;
    uint32_t m_sampleRate;

    // Current EQ configuration (updated from parameter queue)
    EQConfiguration m_currentConfig;

    // Filter bank: [channel][band]
    BiquadFilter m_filters[MAX_CHANNELS][MAX_BANDS];

    // Per-band enable flags (quick check in ProcessAudio)
    bool m_bandEnabled[MAX_BANDS] = {};

    // Number of active bands (avoids iterating unused bands)
    uint32_t m_activeBandCount = 0;

    // Master controls (atomic for cross-thread reads)
    std::atomic<bool>  m_masterEnabled;
    std::atomic<float> m_masterGain;

    // Lock-free queue for parameter updates from UI to audio thread
    LockFreeQueue<ParameterUpdateCommand, 256> m_parameterQueue;
};

} // namespace StudioFeel
