// ============================================================================
// StudioFeel — EQ Parameters JSON Serialization
// ============================================================================
// Minimal JSON serializer/deserializer with no external dependencies.
// Used for IPC payloads between UI and APO, and for preset file storage.
// ============================================================================

#include "../include/EQParameters.h"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace StudioFeel {
namespace Json {

// ============================================================================
// Serialization
// ============================================================================

static std::string EscapeString(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:   oss << c;       break;
        }
    }
    return oss.str();
}

static std::string FloatToString(float value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << value;
    // Remove trailing zeros after decimal point
    std::string result = oss.str();
    size_t dot = result.find('.');
    if (dot != std::string::npos) {
        size_t lastNonZero = result.find_last_not_of('0');
        if (lastNonZero == dot) lastNonZero++;  // Keep at least one decimal
        result.erase(lastNonZero + 1);
    }
    return result;
}

std::string SerializeBand(const EQBandConfig& band, int index) {
    std::ostringstream oss;
    oss << "    {\n";
    oss << "      \"index\": " << index << ",\n";
    oss << "      \"type\": \"" << FilterTypeToString(band.type) << "\",\n";
    oss << "      \"frequencyHz\": " << FloatToString(band.frequency) << ",\n";
    oss << "      \"Q\": " << FloatToString(band.Q) << ",\n";
    oss << "      \"gainDb\": " << FloatToString(band.gainDb) << ",\n";
    oss << "      \"enabled\": " << (band.enabled ? "true" : "false");
    if (!band.label.empty()) {
        oss << ",\n      \"label\": \"" << EscapeString(band.label) << "\"";
    }
    oss << "\n    }";
    return oss.str();
}

std::string SerializeConfiguration(const EQConfiguration& config) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"version\": \"1.0\",\n";
    oss << "  \"masterEnabled\": " << (config.masterEnabled ? "true" : "false") << ",\n";
    oss << "  \"masterGainDb\": " << FloatToString(config.masterGainDb) << ",\n";
    oss << "  \"sampleRate\": " << config.sampleRate << ",\n";
    oss << "  \"bands\": [\n";

    for (size_t i = 0; i < config.bands.size(); ++i) {
        oss << SerializeBand(config.bands[i], static_cast<int>(i));
        if (i + 1 < config.bands.size()) oss << ",";
        oss << "\n";
    }

    oss << "  ]\n";
    oss << "}";
    return oss.str();
}

// ============================================================================
// Deserialization — Minimal JSON Parser
// ============================================================================
// This is a lightweight recursive-descent parser sufficient for our JSON
// format. It handles: objects, arrays, strings, numbers, booleans, null.
// ============================================================================

namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : m_input(input), m_pos(0) {}

    bool ParseConfiguration(EQConfiguration& outConfig) {
        SkipWhitespace();
        if (!Expect('{')) return false;

        outConfig = EQConfiguration{};  // Reset

        while (m_pos < m_input.size()) {
            SkipWhitespace();
            if (Peek() == '}') { Advance(); return true; }

            std::string key;
            if (!ParseString(key)) return false;

            SkipWhitespace();
            if (!Expect(':')) return false;
            SkipWhitespace();

            if (key == "version") {
                std::string ver;
                if (!ParseString(ver)) return false;
                // Version check — we only support 1.0 for now
            }
            else if (key == "masterEnabled") {
                bool val;
                if (!ParseBool(val)) return false;
                outConfig.masterEnabled = val;
            }
            else if (key == "masterGainDb") {
                float val;
                if (!ParseFloat(val)) return false;
                outConfig.masterGainDb = val;
            }
            else if (key == "sampleRate") {
                float val;
                if (!ParseFloat(val)) return false;
                outConfig.sampleRate = static_cast<uint32_t>(val);
            }
            else if (key == "bands") {
                if (!ParseBandsArray(outConfig.bands)) return false;
            }
            else {
                // Skip unknown values
                if (!SkipValue()) return false;
            }

            SkipWhitespace();
            if (Peek() == ',') Advance();
        }
        return false;  // Missing closing brace
    }

private:
    bool ParseBandsArray(std::vector<EQBandConfig>& outBands) {
        if (!Expect('[')) return false;
        outBands.clear();

        SkipWhitespace();
        if (Peek() == ']') { Advance(); return true; }

        while (m_pos < m_input.size()) {
            EQBandConfig band;
            if (!ParseBandObject(band)) return false;
            outBands.push_back(band);

            SkipWhitespace();
            if (Peek() == ']') { Advance(); return true; }
            if (!Expect(',')) return false;
            SkipWhitespace();
        }
        return false;
    }

    bool ParseBandObject(EQBandConfig& outBand) {
        SkipWhitespace();
        if (!Expect('{')) return false;

        outBand = EQBandConfig{};

        while (m_pos < m_input.size()) {
            SkipWhitespace();
            if (Peek() == '}') { Advance(); return true; }

            std::string key;
            if (!ParseString(key)) return false;
            SkipWhitespace();
            if (!Expect(':')) return false;
            SkipWhitespace();

            if (key == "type") {
                std::string typeStr;
                if (!ParseString(typeStr)) return false;
                outBand.type = FilterTypeFromString(typeStr);
            }
            else if (key == "frequencyHz") {
                if (!ParseFloat(outBand.frequency)) return false;
            }
            else if (key == "Q") {
                if (!ParseFloat(outBand.Q)) return false;
            }
            else if (key == "gainDb") {
                if (!ParseFloat(outBand.gainDb)) return false;
            }
            else if (key == "enabled") {
                if (!ParseBool(outBand.enabled)) return false;
            }
            else if (key == "label") {
                if (!ParseString(outBand.label)) return false;
            }
            else if (key == "index") {
                float idx;
                if (!ParseFloat(idx)) return false;
                // Index is informational, not stored in band
            }
            else {
                if (!SkipValue()) return false;
            }

            SkipWhitespace();
            if (Peek() == ',') Advance();
        }
        return false;
    }

    bool ParseString(std::string& out) {
        SkipWhitespace();
        if (!Expect('"')) return false;
        out.clear();

        while (m_pos < m_input.size()) {
            char c = m_input[m_pos++];
            if (c == '"') return true;
            if (c == '\\' && m_pos < m_input.size()) {
                char next = m_input[m_pos++];
                switch (next) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    default:   out += next; break;
                }
            } else {
                out += c;
            }
        }
        return false;  // Unterminated string
    }

    bool ParseFloat(float& out) {
        SkipWhitespace();
        size_t start = m_pos;

        // Accept optional minus, digits, optional decimal point + digits
        if (m_pos < m_input.size() && m_input[m_pos] == '-') m_pos++;
        while (m_pos < m_input.size() && std::isdigit(m_input[m_pos])) m_pos++;
        if (m_pos < m_input.size() && m_input[m_pos] == '.') {
            m_pos++;
            while (m_pos < m_input.size() && std::isdigit(m_input[m_pos])) m_pos++;
        }
        // Optional exponent
        if (m_pos < m_input.size() && (m_input[m_pos] == 'e' || m_input[m_pos] == 'E')) {
            m_pos++;
            if (m_pos < m_input.size() && (m_input[m_pos] == '+' || m_input[m_pos] == '-')) m_pos++;
            while (m_pos < m_input.size() && std::isdigit(m_input[m_pos])) m_pos++;
        }

        if (m_pos == start) return false;
        out = std::stof(m_input.substr(start, m_pos - start));
        return true;
    }

    bool ParseBool(bool& out) {
        SkipWhitespace();
        if (m_input.compare(m_pos, 4, "true") == 0) {
            out = true;
            m_pos += 4;
            return true;
        }
        if (m_input.compare(m_pos, 5, "false") == 0) {
            out = false;
            m_pos += 5;
            return true;
        }
        return false;
    }

    bool SkipValue() {
        SkipWhitespace();
        char c = Peek();
        if (c == '"') {
            std::string dummy;
            return ParseString(dummy);
        }
        if (c == '{') return SkipObject();
        if (c == '[') return SkipArray();
        if (c == 't' || c == 'f') {
            bool dummy;
            return ParseBool(dummy);
        }
        if (c == 'n') {
            if (m_input.compare(m_pos, 4, "null") == 0) { m_pos += 4; return true; }
            return false;
        }
        // Must be a number
        float dummy;
        return ParseFloat(dummy);
    }

    bool SkipObject() {
        if (!Expect('{')) return false;
        int depth = 1;
        while (m_pos < m_input.size() && depth > 0) {
            if (m_input[m_pos] == '{') depth++;
            if (m_input[m_pos] == '}') depth--;
            if (m_input[m_pos] == '"') {
                m_pos++;
                while (m_pos < m_input.size() && m_input[m_pos] != '"') {
                    if (m_input[m_pos] == '\\') m_pos++;
                    m_pos++;
                }
            }
            m_pos++;
        }
        return depth == 0;
    }

    bool SkipArray() {
        if (!Expect('[')) return false;
        int depth = 1;
        while (m_pos < m_input.size() && depth > 0) {
            if (m_input[m_pos] == '[') depth++;
            if (m_input[m_pos] == ']') depth--;
            if (m_input[m_pos] == '"') {
                m_pos++;
                while (m_pos < m_input.size() && m_input[m_pos] != '"') {
                    if (m_input[m_pos] == '\\') m_pos++;
                    m_pos++;
                }
            }
            m_pos++;
        }
        return depth == 0;
    }

    void SkipWhitespace() {
        while (m_pos < m_input.size() &&
               (m_input[m_pos] == ' ' || m_input[m_pos] == '\t' ||
                m_input[m_pos] == '\n' || m_input[m_pos] == '\r')) {
            m_pos++;
        }
    }

    char Peek() const {
        return (m_pos < m_input.size()) ? m_input[m_pos] : '\0';
    }

    void Advance() { m_pos++; }

    bool Expect(char expected) {
        if (Peek() == expected) { Advance(); return true; }
        return false;
    }

    const std::string& m_input;
    size_t m_pos;
};

} // anonymous namespace

bool DeserializeConfiguration(const std::string& json, EQConfiguration& outConfig) {
    JsonParser parser(json);
    bool result = parser.ParseConfiguration(outConfig);
    if (result) {
        outConfig.Clamp();
    }
    return result;
}

bool DeserializeBand(const std::string& json, EQBandConfig& outBand) {
    // Wrap in a configuration object for reuse
    std::string wrapped = "{\"bands\": [" + json + "]}";
    EQConfiguration config;
    if (DeserializeConfiguration(wrapped, config) && !config.bands.empty()) {
        outBand = config.bands[0];
        return true;
    }
    return false;
}

} // namespace Json
} // namespace StudioFeel
