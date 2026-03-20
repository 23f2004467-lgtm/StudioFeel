// ============================================================================
// StudioFeel — APO Implementation
// ============================================================================

#ifdef _WIN32

#include "StudioFeelAPO.h"
#include "../../IPC/include/EQParameters.h"

#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <cstring>

namespace StudioFeel {

// ============================================================================
// Construction / Destruction
// ============================================================================

StudioFeelAPO::StudioFeelAPO()
    : CBaseAudioProcessingObject(const_cast<APO_REG_PROPERTIES&>(g_StudioFeelAPORegProps))
{
}

StudioFeelAPO::~StudioFeelAPO()
{
    if (m_pipeServer) {
        m_pipeServer->Stop();
    }
    if (m_propertyStore) {
        m_propertyStore->Release();
        m_propertyStore = nullptr;
    }
}

// ============================================================================
// IUnknown
// ============================================================================

ULONG STDMETHODCALLTYPE StudioFeelAPO::AddRef()
{
    return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE StudioFeelAPO::Release()
{
    ULONG ref = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

HRESULT STDMETHODCALLTYPE StudioFeelAPO::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject) return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
        *ppvObject = static_cast<IAudioProcessingObject*>(this);
    }
    else if (riid == __uuidof(IAudioProcessingObject)) {
        *ppvObject = static_cast<IAudioProcessingObject*>(this);
    }
    else if (riid == __uuidof(IAudioProcessingObjectRT)) {
        *ppvObject = static_cast<IAudioProcessingObjectRT*>(this);
    }
    else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        *ppvObject = static_cast<IAudioProcessingObjectConfiguration*>(this);
    }
    else if (riid == __uuidof(IAudioSystemEffects)) {
        *ppvObject = static_cast<IAudioSystemEffects2*>(this);
    }
    else if (riid == __uuidof(IAudioSystemEffects2)) {
        *ppvObject = static_cast<IAudioSystemEffects2*>(this);
    }
    else if (riid == IID_StudioFeel_AudioSystemEffects) {
        *ppvObject = static_cast<IAudioSystemEffects2*>(this);
    }
    else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

// ============================================================================
// IAudioProcessingObject::Initialize
// ============================================================================

HRESULT STDMETHODCALLTYPE StudioFeelAPO::Initialize(UINT32 cbDataSize, BYTE* pbyData)
{
    if (!pbyData || cbDataSize == 0) return E_INVALIDARG;

    // Let the base class do its initialization
    HRESULT hr = CBaseAudioProcessingObject::Initialize(cbDataSize, pbyData);
    if (FAILED(hr)) return hr;

    // Parse the init structure to get endpoint info and property store
    ParseInitData(cbDataSize, pbyData);

    // Initialize the DSP engine with default configuration
    EQConfiguration defaultConfig;
    defaultConfig.masterEnabled = true;
    defaultConfig.masterGainDb = 0.0f;
    defaultConfig.sampleRate = 48000;  // Will be updated in LockForProcess

    // Try to load configuration from property store
    if (m_propertyStore) {
        LoadConfigurationFromPropertyStore();
    }

    m_dspEngine.Initialize(2, 48000, defaultConfig);

    // Start the named pipe server for IPC with the UI app
    InitializeIPCServer();

    return S_OK;
}

// ============================================================================
// IAudioProcessingObject::IsInputFormatSupported
// ============================================================================

HRESULT STDMETHODCALLTYPE StudioFeelAPO::IsInputFormatSupported(
    IAudioMediaType* pOppositeFormat,
    IAudioMediaType* pRequestedInputFormat,
    IAudioMediaType** ppSupportedInputFormat)
{
    if (!pRequestedInputFormat || !ppSupportedInputFormat) return E_POINTER;

    *ppSupportedInputFormat = nullptr;

    // Get the requested format details
    UNCOMPRESSEDAUDIOFORMAT requestedFormat;
    HRESULT hr = pRequestedInputFormat->GetUncompressedAudioFormat(&requestedFormat);
    if (FAILED(hr)) return hr;

    // We support:
    //   - Float32 samples only (standard APO format)
    //   - 1-8 channels
    //   - Any sample rate
    bool isSupported = true;

    if (requestedFormat.guidFormatType != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        isSupported = false;
    }
    if (requestedFormat.dwSamplesPerFrame == 0 || requestedFormat.dwSamplesPerFrame > 8) {
        isSupported = false;
    }
    if (requestedFormat.fFramesPerSecond == 0) {
        isSupported = false;
    }

    if (isSupported) {
        // Format is supported as-is
        *ppSupportedInputFormat = pRequestedInputFormat;
        (*ppSupportedInputFormat)->AddRef();
        return S_OK;
    }

    // Format not supported — suggest the closest supported format
    // (float32 with same channel count and sample rate)
    return APOERR_FORMAT_NOT_SUPPORTED;
}

// ============================================================================
// IAudioProcessingObject::GetRegistrationProperties
// ============================================================================

HRESULT STDMETHODCALLTYPE StudioFeelAPO::GetRegistrationProperties(
    APO_REG_PROPERTIES** ppRegProps)
{
    if (!ppRegProps) return E_POINTER;

    *ppRegProps = reinterpret_cast<APO_REG_PROPERTIES*>(
        CoTaskMemAlloc(sizeof(APO_REG_PROPERTIES)));
    if (!*ppRegProps) return E_OUTOFMEMORY;

    memcpy(*ppRegProps, &g_StudioFeelAPORegProps, sizeof(APO_REG_PROPERTIES));
    return S_OK;
}

// ============================================================================
// IAudioProcessingObjectConfiguration
// ============================================================================

HRESULT STDMETHODCALLTYPE StudioFeelAPO::LockForProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
    if (u32NumInputConnections != 1 || u32NumOutputConnections != 1)
        return E_INVALIDARG;

    // Let base class validate connections
    HRESULT hr = CBaseAudioProcessingObject::LockForProcess(
        u32NumInputConnections, ppInputConnections,
        u32NumOutputConnections, ppOutputConnections);
    if (FAILED(hr)) return hr;

    // Extract format details from the input connection
    UNCOMPRESSEDAUDIOFORMAT inputFormat;
    hr = ppInputConnections[0]->pFormat->GetUncompressedAudioFormat(&inputFormat);
    if (FAILED(hr)) return hr;

    m_numChannels   = inputFormat.dwSamplesPerFrame;
    m_sampleRate    = static_cast<uint32_t>(inputFormat.fFramesPerSecond);
    m_bitsPerSample = inputFormat.dwBitsPerSample;

    // Re-initialize DSP engine with actual format
    EQConfiguration config = m_dspEngine.GetCurrentConfig();
    config.sampleRate = m_sampleRate;
    m_dspEngine.Initialize(m_numChannels, m_sampleRate, config);

    m_isLocked = true;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE StudioFeelAPO::UnlockForProcess()
{
    m_isLocked = false;
    return CBaseAudioProcessingObject::UnlockForProcess();
}

// ============================================================================
// IAudioProcessingObjectRT::APOProcess
// ============================================================================
// CRITICAL: This runs on a real-time thread managed by MMCSS.
// Do NOT allocate memory, acquire locks, or make system calls here.
// ============================================================================

void STDMETHODCALLTYPE StudioFeelAPO::APOProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_PROPERTY** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_PROPERTY** ppOutputConnections)
{
    // Validate connections
    if (u32NumInputConnections != 1 || u32NumOutputConnections != 1) return;

    APO_CONNECTION_PROPERTY* pInputConn  = ppInputConnections[0];
    APO_CONNECTION_PROPERTY* pOutputConn = ppOutputConnections[0];

    // If input is silent, propagate silence
    if (pInputConn->u32BufferFlags == BUFFER_SILENT) {
        pOutputConn->u32BufferFlags = BUFFER_SILENT;
        pOutputConn->u32ValidFrameCount = pInputConn->u32ValidFrameCount;
        return;
    }

    // Get pointers to audio buffers
    float* pInputBuffer  = reinterpret_cast<float*>(pInputConn->pBuffer);
    float* pOutputBuffer = reinterpret_cast<float*>(pOutputConn->pBuffer);
    const uint32_t frameCount = pInputConn->u32ValidFrameCount;

    // Copy input to output if they're different buffers
    if (pInputBuffer != pOutputBuffer) {
        const uint32_t totalSamples = frameCount * m_numChannels;
        memcpy(pOutputBuffer, pInputBuffer, totalSamples * sizeof(float));
    }

    // Process the audio through the EQ filter chain
    // (This internally drains the parameter update queue first)
    m_dspEngine.ProcessAudio(pOutputBuffer, frameCount);

    // Mark output as valid
    pOutputConn->u32BufferFlags = BUFFER_VALID;
    pOutputConn->u32ValidFrameCount = frameCount;
}

// ============================================================================
// IAudioSystemEffects2::GetEffectsList
// ============================================================================

HRESULT STDMETHODCALLTYPE StudioFeelAPO::GetEffectsList(
    LPGUID* ppEffectsIds, UINT* pcEffects, HANDLE Event)
{
    if (!ppEffectsIds || !pcEffects) return E_POINTER;

    // Report our single effect (the EQ)
    *pcEffects = 1;
    *ppEffectsIds = reinterpret_cast<LPGUID>(
        CoTaskMemAlloc(sizeof(GUID)));
    if (!*ppEffectsIds) return E_OUTOFMEMORY;

    (*ppEffectsIds)[0] = CLSID_StudioFeel_EFX;
    return S_OK;
}

// ============================================================================
// Internal: Parse Init Data
// ============================================================================

void StudioFeelAPO::ParseInitData(UINT32 cbDataSize, BYTE* pbyData)
{
    // The init structure depends on the Windows version:
    //   - Windows 10/11 pre-22H2: APOInitSystemEffects2
    //   - Windows 11 22H2+:       APOInitSystemEffects3
    //
    // We check the size to determine which struct we received.
    // APOInitSystemEffects3 is larger and includes CAPX property store info.

    if (cbDataSize >= sizeof(APOInitSystemEffects2)) {
        auto* pInit2 = reinterpret_cast<APOInitSystemEffects2*>(pbyData);

        // Extract endpoint ID from the device property store
        if (pInit2->pDeviceCollection) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pInit2->pDeviceCollection->Item(0, &pDevice)) && pDevice) {
                LPWSTR deviceId = nullptr;
                if (SUCCEEDED(pDevice->GetId(&deviceId)) && deviceId) {
                    // Convert wide string to narrow for our IPC layer
                    int len = WideCharToMultiByte(CP_UTF8, 0, deviceId, -1,
                                                   nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        m_endpointId.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, deviceId, -1,
                                            m_endpointId.data(), len, nullptr, nullptr);
                    }
                    CoTaskMemFree(deviceId);
                }
                pDevice->Release();
            }
        }

        // Check if this is actually an APOInitSystemEffects3 (Windows 11 22H2+)
        // by checking if the struct is large enough
        // Note: In production, check for APOInitSystemEffects3 availability
        // via the presence of the CAPX property store pointer.
        m_isWindows11 = false;  // Default; set to true if we detect Win11 CAPX
    }
}

// ============================================================================
// Internal: Initialize IPC Server
// ============================================================================

void StudioFeelAPO::InitializeIPCServer()
{
    if (m_endpointId.empty()) return;

    m_pipeServer = std::make_unique<NamedPipeServer>();

    // Set up request handler that bridges IPC to DSP engine
    m_pipeServer->SetRequestHandler(
        [this](PipeMessageType type, const std::string& payload) -> std::string {
            return HandleIPCRequest(type, payload);
        });

    m_pipeServer->Start(m_endpointId);
}

// ============================================================================
// Internal: Save Configuration to Property Store
// ============================================================================

void StudioFeelAPO::SaveConfigurationToPropertyStore()
{
    if (!m_propertyStore) return;

    // Get current configuration
    const auto& config = m_dspEngine.GetCurrentConfig();

    // Serialize to JSON
    std::string json = Json::SerializeConfiguration(config);

    // Convert to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, json.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return;

    std::vector<wchar_t> wideJson(wideLen);
    int result = MultiByteToWideChar(CP_UTF8, 0, json.c_str(), -1, wideJson.data(), wideLen);
    if (result != wideLen) return;

    // Store in the property store
    // We use a custom property key format: {STUDIOFEEL_PROPERTYKEY_CATEGORY, PID_STUDIOFEEL_MASTER_ENABLED + 1}
    // For full implementation, we'd use a better persistence strategy

    // Create a property key for the full configuration
    PROPERTYKEY key;
    key.fmtid = STUDIOFEEL_PROPERTYKEY_CATEGORY;
    key.pid = PID_STUDIOFEEL_MASTER_ENABLED + 100;  // Full config storage

    // Allocate string with CoTaskMemAlloc (required for VT_LPWSTR in PROPVARIANT)
    PWSTR persistentStr = nullptr;
    HRESULT hr = SHStrDupW(wideJson.data(), &persistentStr);
    if (FAILED(hr) || !persistentStr) return;

    PROPVARIANT variant = {};
    variant.vt = VT_LPWSTR;
    variant.pwszVal = persistentStr;

    // Attempt to save (may fail if property store is read-only)
    PROPVARIANT variantOld = {};
    m_propertyStore->GetValue(key.fmtid, key.pid, &variantOld);
    m_propertyStore->SetValue(key.fmtid, key.pid, &variant);

    PropVariantClear(&variantOld);  // Frees old value
    PropVariantClear(&variant);     // Frees persistentStr
}

// ============================================================================
// Internal: Load Configuration from Property Store
// ============================================================================

void StudioFeelAPO::LoadConfigurationFromPropertyStore()
{
    if (!m_propertyStore) return;

    // Try to read saved configuration
    PROPERTYKEY key;
    key.fmtid = STUDIOFEEL_PROPERTYKEY_CATEGORY;
    key.pid = PID_STUDIOFEEL_MASTER_ENABLED + 100;  // Full config storage

    PROPVARIANT variant = {};
    HRESULT hr = m_propertyStore->GetValue(key.fmtid, key.pid, &variant);

    if (SUCCEEDED(hr) && variant.vt == VT_LPWSTR && variant.pwszVal) {
        // Convert wide string to narrow
        int len = WideCharToMultiByte(CP_UTF8, 0, variant.pwszVal, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string json(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, variant.pwszVal, -1, &json[0], len, nullptr, nullptr);

            // Parse and apply
            EQConfiguration savedConfig;
            if (Json::DeserializeConfiguration(json, savedConfig)) {
                m_dspEngine.UpdateFullConfiguration(savedConfig);
            }
        }
        PropVariantClear(&variant);
    }
}

// ============================================================================
// Internal: Process Individual Parameter Updates
// ============================================================================

std::string StudioFeelAPO::ProcessParameterUpdate(const std::string& key, const std::string& value)
{
    // Parse the key and update the corresponding parameter
    // Format: "master.enabled", "master.gain", "band.0.gain", etc.

    if (key == "master.enabled") {
        bool enabled = (value == "true" || value == "1");
        auto config = m_dspEngine.GetCurrentConfig();
        config.masterEnabled = enabled;
        m_dspEngine.UpdateFullConfiguration(config);
        SaveConfigurationToPropertyStore();
        return "{}";
    }

    if (key == "master.gain") {
        try {
            float gain = std::stof(value);
            auto config = m_dspEngine.GetCurrentConfig();
            config.masterGainDb = gain;
            m_dspEngine.UpdateFullConfiguration(config);
            SaveConfigurationToPropertyStore();
            return "{}";
        } catch (...) {
            return "{\"error\": \"Invalid gain value\"}";
        }
    }

    // Band parameters: "band.N.param" where N is band index, param is property name
    if (key.compare(0, 5, "band.") == 0) {
        size_t secondDot = key.find('.', 5);
        if (secondDot == std::string::npos) {
            return "{\"error\": \"Invalid band key format\"}";
        }

        // Parse band index
        int bandIndex = std::stoi(key.substr(5, secondDot - 5));
        std::string paramName = key.substr(secondDot + 1);

        auto config = m_dspEngine.GetCurrentConfig();
        if (bandIndex < 0 || bandIndex >= static_cast<int>(config.bands.size())) {
            return "{\"error\": \"Band index out of range\"}";
        }

        auto& band = config.bands[bandIndex];

        if (paramName == "enabled") {
            band.enabled = (value == "true" || value == "1");
        }
        else if (paramName == "gain") {
            try {
                band.gainDb = std::stof(value);
            } catch (...) {
                return "{\"error\": \"Invalid gain value\"}";
            }
        }
        else if (paramName == "frequency") {
            try {
                band.frequency = std::stof(value);
                band.frequency = std::clamp(band.frequency, 20.0f, 20000.0f);
            } catch (...) {
                return "{\"error\": \"Invalid frequency value\"}";
            }
        }
        else if (paramName == "Q") {
            try {
                band.Q = std::stof(value);
                band.Q = std::clamp(band.Q, 0.1f, 10.0f);
            } catch (...) {
                return "{\"error\": \"Invalid Q value\"}";
            }
        }
        else if (paramName == "type") {
            band.type = FilterTypeFromString(value);
        }
        else {
            return "{\"error\": \"Unknown parameter name\"}";
        }

        m_dspEngine.UpdateFullConfiguration(config);
        SaveConfigurationToPropertyStore();
        return "{}";
    }

    return "{\"error\": \"Unknown key\"}";
}

// ============================================================================
// Internal: Handle IPC Requests
// ============================================================================

std::string StudioFeelAPO::HandleIPCRequest(PipeMessageType type,
                                              const std::string& payload)
{
    switch (type) {
        case PipeMessageType::GetConfig: {
            // Serialize current config and return
            const auto& config = m_dspEngine.GetCurrentConfig();
            return Json::SerializeConfiguration(config);
        }

        case PipeMessageType::SetConfig: {
            // Parse and apply full configuration
            EQConfiguration newConfig;
            if (Json::DeserializeConfiguration(payload, newConfig)) {
                m_dspEngine.UpdateFullConfiguration(newConfig);
                return "{}";  // Success
            }
            return "{\"error\": \"Invalid configuration JSON\"}";
        }

        case PipeMessageType::SetParameter: {
            // Parse individual parameter update
            // Expected payload: {"key": "band.0.gain", "value": 6.0}
            // Supported keys:
            //   - master.enabled
            //   - master.gain
            //   - band.N.enabled (N = 0-9)
            //   - band.N.gain
            //   - band.N.frequency
            //   - band.N.Q
            //   - band.N.type

            // Simple JSON parsing for key/value
            std::string key;
            std::string valueStr;

            // Extract key
            size_t keyPos = payload.find("\"key\":");
            if (keyPos == std::string::npos) return "{\"error\": \"Missing key\"}";
            keyPos = payload.find("\"", keyPos + 6);
            if (keyPos == std::string::npos) return "{\"error\": \"Invalid key format\"}";
            size_t keyStart = keyPos + 1;
            keyPos = payload.find("\"", keyStart);
            if (keyPos == std::string::npos) return "{\"error\": \"Invalid key format\"}";
            key = payload.substr(keyStart, keyPos - keyStart);

            // Extract value (handle both strings and numbers)
            size_t valuePos = payload.find("\"value\":");
            if (valuePos == std::string::npos) return "{\"error\": \"Missing value\"}";

            // Skip past "value":
            valuePos += 7;

            // Skip whitespace and colon
            while (valuePos < payload.length() && (payload[valuePos] == ' ' || payload[valuePos] == '\t' || payload[valuePos] == ':')) {
                valuePos++;
            }

            // Check if value is a string
            if (valuePos < payload.length() && payload[valuePos] == '"') {
                size_t valueStart = valuePos + 1;
                size_t valueEnd = payload.find("\"", valueStart);
                if (valueEnd == std::string::npos) return "{\"error\": \"Invalid value format\"}";
                valueStr = payload.substr(valueStart, valueEnd - valueStart);
            } else {
                // Number - extract until comma or closing brace
                size_t valueStart = valuePos;
                while (valuePos < payload.length() && payload[valuePos] != ',' && payload[valuePos] != '}' && payload[valuePos] != ' ') {
                    valuePos++;
                }
                valueStr = payload.substr(valueStart, valuePos - valueStart);
            }

            // Process the key/value pair
            return ProcessParameterUpdate(key, valueStr);
        }

        default:
            return "{\"error\": \"Unknown message type\"}";
    }
}

} // namespace StudioFeel

#endif // _WIN32
