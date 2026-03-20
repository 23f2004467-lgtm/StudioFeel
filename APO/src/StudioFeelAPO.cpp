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
        // TODO: Read saved configuration from property store
        // For now, use default flat configuration
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
            // TODO: Implement individual parameter parsing
            return "{}";
        }

        default:
            return "{\"error\": \"Unknown message type\"}";
    }
}

} // namespace StudioFeel

#endif // _WIN32
