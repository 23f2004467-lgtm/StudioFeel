// ============================================================================
// StudioFeel — Preset System Implementation
// ============================================================================
// Handles factory presets, user presets, and import/export of .studiofeel files.
// ============================================================================

#include "../include/PresetFormat.h"
#include "../include/EQParameters.h"

#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <random>
#include <ctime>
#include <algorithm>

namespace StudioFeel {

// ============================================================================
// Preset JSON Serialization
// ============================================================================

namespace PresetJson {

std::string Serialize(const Preset& preset) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"id\": \"" << preset.id << "\",\n";
    oss << "  \"name\": \"" << preset.name << "\",\n";
    oss << "  \"description\": \"" << preset.description << "\",\n";
    oss << "  \"category\": \"" << preset.category << "\",\n";
    oss << "  \"createdAt\": " << preset.createdAt << ",\n";
    oss << "  \"modifiedAt\": " << preset.modifiedAt << ",\n";
    oss << "  \"eqConfig\": " << Json::SerializeConfiguration(preset.eqConfig) << "\n";
    oss << "}";
    return oss.str();
}

bool Deserialize(const std::string& json, Preset& outPreset) {
    // Minimal JSON parser for preset format
    // We'll parse key-value pairs manually to avoid external deps

    outPreset = Preset{};  // Reset

    // Helper to find a key's value in JSON
    auto findValue = [&json](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";

        pos += searchKey.length();

        // Skip whitespace
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
            pos++;
        }

        // Check if value is a string (starts with quote)
        if (pos < json.length() && json[pos] == '"') {
            pos++;  // Skip opening quote
            std::string result;
            while (pos < json.length() && json[pos] != '"' && json[pos] != '\n') {
                if (json[pos] == '\\' && pos + 1 < json.length()) {
                    pos++;  // Skip escape char
                    result += json[pos++];
                } else {
                    result += json[pos++];
                }
            }
            return result;
        }

        // For numbers and objects, find the end
        size_t endPos = pos;
        int braceLevel = 0;
        int bracketLevel = 0;

        while (endPos < json.length()) {
            if (json[endPos] == '{') braceLevel++;
            else if (json[endPos] == '}') braceLevel--;
            else if (json[endPos] == '[') bracketLevel++;
            else if (json[endPos] == ']') bracketLevel--;
            else if (json[endPos] == ',' && braceLevel == 0 && bracketLevel == 0) {
                break;  // End of this value
            }
            endPos++;
        }

        return json.substr(pos, endPos - pos);
    };

    // Parse basic fields
    outPreset.id = findValue("id");
    outPreset.name = findValue("name");
    outPreset.description = findValue("description");
    outPreset.category = findValue("category");

    // Parse timestamps
    try {
        std::string createdAtStr = findValue("createdAt");
        std::string modifiedAtStr = findValue("modifiedAt");
        outPreset.createdAt = std::stoull(createdAtStr);
        outPreset.modifiedAt = std::stoull(modifiedAtStr);
    } catch (...) {
        outPreset.createdAt = CurrentTimestamp();
        outPreset.modifiedAt = outPreset.createdAt;
    }

    // Parse EQ configuration
    std::string eqConfigKey = "\"eqConfig\":";
    size_t eqPos = json.find(eqConfigKey);
    if (eqPos != std::string::npos) {
        eqPos += eqConfigKey.length();

        // Find the matching closing brace
        size_t endPos = eqPos;
        int braceLevel = 0;
        while (endPos < json.length()) {
            if (json[endPos] == '{') braceLevel++;
            else if (json[endPos] == '}') {
                braceLevel--;
                if (braceLevel < 0) break;
            }
            endPos++;
        }

        std::string eqJson = json.substr(eqPos, endPos - eqPos);
        Json::DeserializeConfiguration(eqJson, outPreset.eqConfig);
    }

    return !outPreset.id.empty();
}

std::string SerializeArray(const std::vector<Preset>& presets) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"version\": \"1.0\",\n";
    oss << "  \"presets\": [\n";

    for (size_t i = 0; i < presets.size(); ++i) {
        oss << "    " << Serialize(presets[i]);
        if (i + 1 < presets.size()) oss << ",";
        oss << "\n";
    }

    oss << "  ]\n";
    oss << "}";
    return oss.str();
}

bool DeserializeArray(const std::string& json, std::vector<Preset>& outPresets) {
    outPresets.clear();

    // Find the presets array
    size_t arrayStart = json.find("\"presets\": [");
    if (arrayStart == std::string::npos) return false;

    arrayStart = json.find('[', arrayStart);
    size_t arrayEnd = json.rfind(']');
    if (arrayEnd == std::string::npos || arrayEnd <= arrayStart) return false;

    // Extract array content and parse individual presets
    std::string arrayContent = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

    // Simple parsing: split by "}," pattern
    size_t pos = 0;
    size_t presetStart = 0;

    int braceLevel = 0;
    while (pos < arrayContent.length()) {
        if (arrayContent[pos] == '{') {
            if (braceLevel == 0) presetStart = pos;
            braceLevel++;
        } else if (arrayContent[pos] == '}') {
            braceLevel--;
            if (braceLevel == 0) {
                // Found a complete preset
                std::string presetJson = arrayContent.substr(presetStart, pos - presetStart + 1);
                Preset preset;
                if (Deserialize(presetJson, preset)) {
                    outPresets.push_back(preset);
                }
            }
        }
        pos++;
    }

    return !outPresets.empty();
}

} // namespace PresetJson

// ============================================================================
// Preset Manager
// ============================================================================

PresetManager::PresetManager() = default;

PresetManager::~PresetManager() {
    // Save user presets before exit
    if (!m_userPresetsPath.empty()) {
        PersistUserPresets();
    }
}

bool PresetManager::LoadFactoryPresets(const std::string& factoryPresetsDir) {
    namespace fs = std::filesystem;

    m_factoryPresets.clear();

    // Try to load factory_presets.json
    std::string factoryFile = factoryPresetsDir;
    if (!factoryFile.empty() && factoryFile.back() != '/' && factoryFile.back() != '\\') {
        factoryFile += '/';
    }
    factoryFile += "factory_presets.json";

    std::ifstream file(factoryFile);
    if (!file.is_open()) {
        // Use default embedded presets
        // For now, return true with empty list
        return true;
    }

    // Read entire file
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Parse as JSON array of presets
    // Look for "presets": [...] array
    size_t arrayStart = content.find("\"presets\":");
    if (arrayStart == std::string::npos) return false;

    arrayStart = content.find('[', arrayStart);
    size_t arrayEnd = content.rfind(']');
    if (arrayEnd == std::string::npos) return false;

    std::string arrayContent = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

    // Parse individual presets (simplified parser)
    size_t pos = 0;
    size_t presetStart = 0;
    int braceLevel = 0;

    while (pos < arrayContent.length()) {
        if (arrayContent[pos] == '{') {
            if (braceLevel == 0) presetStart = pos;
            braceLevel++;
        } else if (arrayContent[pos] == '}') {
            braceLevel--;
            if (braceLevel == 0) {
                std::string presetJson = arrayContent.substr(presetStart, pos - presetStart + 1);

                Preset preset;
                if (PresetJson::Deserialize(presetJson, preset)) {
                    m_factoryPresets.push_back(preset);
                }
            }
        }
        pos++;
    }

    return true;
}

bool PresetManager::LoadUserPresets(const std::string& userPresetsPath) {
    m_userPresetsPath = userPresetsPath;
    m_userPresets.clear();

    std::ifstream file(userPresetsPath);
    if (!file.is_open()) {
        // File doesn't exist yet - not an error
        return true;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    return PresetJson::DeserializeArray(content, m_userPresets);
}

std::vector<const Preset*> PresetManager::GetAllPresets() const {
    std::vector<const Preset*> result;

    for (const auto& preset : m_factoryPresets) {
        result.push_back(&preset);
    }
    for (const auto& preset : m_userPresets) {
        result.push_back(&preset);
    }

    return result;
}

const Preset* PresetManager::FindById(const std::string& id) const {
    // Search factory presets first
    for (const auto& preset : m_factoryPresets) {
        if (preset.id == id) return &preset;
    }

    // Search user presets
    for (const auto& preset : m_userPresets) {
        if (preset.id == id) return &preset;
    }

    return nullptr;
}

bool PresetManager::SaveUserPreset(Preset& preset) {
    // Generate ID if empty
    if (preset.id.empty()) {
        preset.id = GenerateUUID();
    }

    preset.category = "user";

    uint64_t now = CurrentTimestamp();
    preset.createdAt = (preset.createdAt == 0) ? now : preset.createdAt;
    preset.modifiedAt = now;

    // Check if updating existing preset
    for (auto& existing : m_userPresets) {
        if (existing.id == preset.id) {
            existing = preset;
            return PersistUserPresets();
        }
    }

    // Add new preset
    m_userPresets.push_back(preset);
    return PersistUserPresets();
}

bool PresetManager::UpdateUserPreset(const Preset& preset) {
    for (auto& existing : m_userPresets) {
        if (existing.id == preset.id) {
            existing = preset;
            existing.modifiedAt = CurrentTimestamp();
            return PersistUserPresets();
        }
    }
    return false;  // Not found
}

bool PresetManager::DeleteUserPreset(const std::string& id) {
    auto it = std::find_if(m_userPresets.begin(), m_userPresets.end(),
                           [&id](const Preset& p) { return p.id == id; });

    if (it != m_userPresets.end()) {
        m_userPresets.erase(it);
        return PersistUserPresets();
    }

    return false;  // Not found or is factory preset
}

bool PresetManager::ImportPreset(const std::string& filePath, Preset& outPreset) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    if (!PresetJson::Deserialize(content, outPreset)) {
        return false;
    }

    // Mark as imported and generate new ID
    outPreset.category = "imported";
    outPreset.id = GenerateUUID();

    return SaveUserPreset(outPreset);
}

bool PresetManager::ExportPreset(const Preset& preset, const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    std::string json = PresetJson::Serialize(preset);
    file << json;
    file.close();

    return true;
}

bool PresetManager::PersistUserPresets() {
    if (m_userPresetsPath.empty()) return false;

    // Ensure directory exists
    namespace fs = std::filesystem;
    fs::path filePath(m_userPresetsPath);
    fs::path dirPath = filePath.parent_path();

    if (!dirPath.empty()) {
        std::error_code ec;
        fs::create_directories(dirPath, ec);
    }

    std::ofstream file(m_userPresetsPath);
    if (!file.is_open()) return false;

    std::string json = PresetJson::SerializeArray(m_userPresets);
    file << json;
    file.close();

    // Notify callback if set
    if (m_changeCallback) {
        m_changeCallback();
    }

    return true;
}

std::string PresetManager::GenerateUUID() {
    // Simple UUID v4 generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::ostringstream oss;
    oss << std::hex;

    for (int i = 0; i < 8; i++) {
        oss << dis(gen);
    }
    oss << "-";

    for (int i = 0; i < 4; i++) {
        oss << dis(gen);
    }
    oss << "-4";  // Version 4

    for (int i = 0; i < 3; i++) {
        oss << dis(gen);
    }
    oss << "-";

    oss << dis2(gen);  // Variant
    for (int i = 0; i < 3; i++) {
        oss << dis(gen);
    }
    oss << "-";

    for (int i = 0; i < 12; i++) {
        oss << dis(gen);
    }

    return oss.str();
}

uint64_t PresetManager::CurrentTimestamp() {
    return static_cast<uint64_t>(std::time(nullptr));
}

} // namespace StudioFeel
