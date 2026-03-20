#pragma once
// ============================================================================
// StudioFeel — Preset Data Model and Manager
// ============================================================================
// Handles factory presets (embedded in MSIX), user presets (stored in
// LocalAppData), and import/export of .studiofeel preset files.
//
// File format: JSON with metadata envelope wrapping an EQConfiguration.
// ============================================================================

#include "EQParameters.h"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace StudioFeel {

// ============================================================================
// Preset Data Structure
// ============================================================================

struct Preset {
    std::string id;              // UUID (e.g., "bass_boost" for factory, UUID for user)
    std::string name;            // Display name
    std::string description;     // Short description
    std::string category;        // "factory" | "user" | "imported"
    uint64_t    createdAt  = 0;  // Unix timestamp (seconds)
    uint64_t    modifiedAt = 0;  // Unix timestamp (seconds)

    EQConfiguration eqConfig;

    // Optional: pre-computed 64-point frequency response for thumbnail display
    std::vector<float> frequencyResponseCurve;

    bool IsFactory() const { return category == "factory"; }
    bool IsUser()    const { return category == "user" || category == "imported"; }
};

// ============================================================================
// Preset Serialization
// ============================================================================

namespace PresetJson {

// Serialize a single preset to JSON (for .studiofeel file or storage)
std::string Serialize(const Preset& preset);

// Deserialize a single preset from JSON
bool Deserialize(const std::string& json, Preset& outPreset);

// Serialize an array of presets (for the user presets file)
std::string SerializeArray(const std::vector<Preset>& presets);

// Deserialize an array of presets
bool DeserializeArray(const std::string& json, std::vector<Preset>& outPresets);

} // namespace PresetJson

// ============================================================================
// Preset Manager
// ============================================================================

class PresetManager {
public:
    PresetManager();
    ~PresetManager();

    // ========================================================================
    // Initialization
    // ========================================================================

    // Load factory presets from embedded resources.
    // factoryPresetsDir: path to Assets/presets/ directory in MSIX package
    bool LoadFactoryPresets(const std::string& factoryPresetsDir);

    // Load user presets from LocalAppData.
    // userPresetsPath: path to presets.json in app's LocalState folder
    bool LoadUserPresets(const std::string& userPresetsPath);

    // ========================================================================
    // Preset Access
    // ========================================================================

    const std::vector<Preset>& GetFactoryPresets() const { return m_factoryPresets; }
    const std::vector<Preset>& GetUserPresets()    const { return m_userPresets; }

    // Get all presets (factory + user)
    std::vector<const Preset*> GetAllPresets() const;

    // Find a preset by ID (searches both factory and user)
    const Preset* FindById(const std::string& id) const;

    // ========================================================================
    // User Preset CRUD
    // ========================================================================

    // Save a new user preset. Generates a UUID if preset.id is empty.
    // Persists to disk immediately.
    bool SaveUserPreset(Preset& preset);

    // Update an existing user preset (must already exist in user presets).
    bool UpdateUserPreset(const Preset& preset);

    // Delete a user preset by ID. Cannot delete factory presets.
    bool DeleteUserPreset(const std::string& id);

    // ========================================================================
    // Import / Export
    // ========================================================================

    // Import a preset from a .studiofeel file.
    // Sets category to "imported" and generates a new ID.
    bool ImportPreset(const std::string& filePath, Preset& outPreset);

    // Export a preset to a .studiofeel file.
    bool ExportPreset(const Preset& preset, const std::string& filePath);

    // ========================================================================
    // Change Notification
    // ========================================================================

    using PresetChangeCallback = std::function<void()>;
    void OnPresetsChanged(PresetChangeCallback callback) { m_changeCallback = callback; }

private:
    // Persist user presets to disk
    bool PersistUserPresets();

    // Generate a UUID for new presets
    static std::string GenerateUUID();

    // Get current Unix timestamp
    static uint64_t CurrentTimestamp();

    std::vector<Preset> m_factoryPresets;
    std::vector<Preset> m_userPresets;
    std::string m_userPresetsPath;
    PresetChangeCallback m_changeCallback;
};

} // namespace StudioFeel
