#pragma once
// ============================================================================
// StudioFeel — EQ Parameter Structures and JSON Serialization
// ============================================================================
// Shared between APO, IPC, and UI layers. All EQ configuration state lives here.
// JSON format used for IPC payloads, preset storage, and property store values.
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace StudioFeel {

// ============================================================================
// Filter Types
// ============================================================================

enum class FilterType : uint32_t {
    Peaking   = 0,   // Parametric bell/peak EQ
    LowShelf  = 1,   // Low-frequency shelf
    HighShelf = 2,   // High-frequency shelf
    Lowpass   = 3,   // Low-pass filter (cuts highs)
    Highpass  = 4,   // High-pass filter (cuts lows)
    Notch     = 5    // Narrow band rejection
};

inline const char* FilterTypeToString(FilterType type) {
    switch (type) {
        case FilterType::Peaking:   return "peaking";
        case FilterType::LowShelf:  return "lowshelf";
        case FilterType::HighShelf: return "highshelf";
        case FilterType::Lowpass:   return "lowpass";
        case FilterType::Highpass:  return "highpass";
        case FilterType::Notch:     return "notch";
        default:                    return "peaking";
    }
}

inline FilterType FilterTypeFromString(const std::string& str) {
    if (str == "peaking")   return FilterType::Peaking;
    if (str == "lowshelf")  return FilterType::LowShelf;
    if (str == "highshelf") return FilterType::HighShelf;
    if (str == "lowpass")   return FilterType::Lowpass;
    if (str == "highpass")  return FilterType::Highpass;
    if (str == "notch")     return FilterType::Notch;
    return FilterType::Peaking;
}

// ============================================================================
// EQ Band Configuration
// ============================================================================

struct EQBandConfig {
    FilterType type       = FilterType::Peaking;
    float      frequency  = 1000.0f;   // Hz, range: 20.0 - 20000.0
    float      Q          = 1.0f;      // Quality factor, range: 0.1 - 10.0
    float      gainDb     = 0.0f;      // dB, range: -24.0 - +24.0
    bool       enabled    = true;
    std::string label;                  // User-friendly name (e.g., "Bass", "Presence")

    // Clamp all values to valid ranges
    void Clamp() {
        frequency = std::clamp(frequency, 20.0f, 20000.0f);
        Q         = std::clamp(Q, 0.1f, 10.0f);
        gainDb    = std::clamp(gainDb, -24.0f, 24.0f);
    }

    bool operator==(const EQBandConfig& other) const {
        return type == other.type
            && std::abs(frequency - other.frequency) < 0.01f
            && std::abs(Q - other.Q) < 0.001f
            && std::abs(gainDb - other.gainDb) < 0.01f
            && enabled == other.enabled;
    }

    bool operator!=(const EQBandConfig& other) const { return !(*this == other); }
};

// ============================================================================
// Full EQ Configuration
// ============================================================================

struct EQConfiguration {
    static constexpr int MAX_BANDS = 10;

    std::vector<EQBandConfig> bands;
    bool     masterEnabled = true;
    float    masterGainDb  = 0.0f;      // -12.0 to +12.0 dB
    uint32_t sampleRate    = 48000;

    void Clamp() {
        masterGainDb = std::clamp(masterGainDb, -12.0f, 12.0f);
        for (auto& band : bands) band.Clamp();
        if (bands.size() > MAX_BANDS) bands.resize(MAX_BANDS);
    }

    bool operator==(const EQConfiguration& other) const {
        return masterEnabled == other.masterEnabled
            && std::abs(masterGainDb - other.masterGainDb) < 0.01f
            && sampleRate == other.sampleRate
            && bands == other.bands;
    }
};

// ============================================================================
// Parameter Update Commands (for lock-free queue between UI and APO)
// ============================================================================

struct ParameterUpdateCommand {
    enum class Type : uint32_t {
        SetBandGain,
        SetBandFrequency,
        SetBandQ,
        SetBandType,
        SetBandEnabled,
        SetMasterGain,
        SetMasterEnabled,
        SetSampleRate,
        FullConfiguration
    };

    Type     commandType   = Type::SetBandGain;
    int32_t  bandIndex     = -1;         // Which band (-1 for master/global)
    float    floatValue    = 0.0f;
    bool     boolValue     = false;
    uint32_t uintValue     = 0;

    // Used only for FullConfiguration command type.
    // NOTE: This makes ParameterUpdateCommand non-trivially-copyable.
    // For the lock-free queue, only FullConfiguration commands use this field.
    EQConfiguration fullConfig;
};

// ============================================================================
// Audio Endpoint Information
// ============================================================================

struct AudioEndpointInfo {
    std::string id;              // Windows endpoint ID string
    std::string friendlyName;    // Human-readable name (e.g., "Speakers (Realtek)")
    bool        isDefault = false;
    bool        isActive  = false;
};

// ============================================================================
// JSON Serialization
// ============================================================================
// Lightweight JSON generation without external dependencies.
// For parsing, we use a minimal recursive-descent parser.
// ============================================================================

namespace Json {

// Serialize a single band to JSON
std::string SerializeBand(const EQBandConfig& band, int index);

// Serialize full configuration to JSON
std::string SerializeConfiguration(const EQConfiguration& config);

// Parse full configuration from JSON string
// Returns true on success, false on parse error
bool DeserializeConfiguration(const std::string& json, EQConfiguration& outConfig);

// Parse a single band from a JSON object substring
bool DeserializeBand(const std::string& json, EQBandConfig& outBand);

} // namespace Json
} // namespace StudioFeel
