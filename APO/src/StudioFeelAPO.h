#pragma once
// ============================================================================
// StudioFeel — Audio Processing Object (APO) COM Class
// ============================================================================
// Implements the Windows Audio Processing Object interfaces to provide
// system-wide equalizer functionality. Loaded into audiodg.exe by the
// Windows Audio Engine.
//
// APO Type: EFX (Endpoint Effects) — processes final mixed output.
//
// Interfaces implemented:
//   - IAudioProcessingObject           (initialization, format negotiation)
//   - IAudioProcessingObjectRT         (real-time audio processing)
//   - IAudioProcessingObjectConfiguration (lock/unlock for process)
//   - IAudioSystemEffects2             (identifies as system effect APO)
//
// Base class: CBaseAudioProcessingObject (from audioenginebaseapo.lib)
//   Provides default implementations; we override key methods.
// ============================================================================

#ifdef _WIN32

#include <windows.h>
#include <unknwn.h>
#include <audioenginebaseapo.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include "APOGuids.h"
#include "DSPEngine.h"
#include "../../IPC/include/NamedPipeIPC.h"

#include <atomic>
#include <memory>
#include <string>

namespace StudioFeel {

// ============================================================================
// APO Registration Properties
// ============================================================================
// These are passed to the audio engine during APO registration via the INF.
// ============================================================================

static const APO_REG_PROPERTIES g_StudioFeelAPORegProps = {
    CLSID_StudioFeel_EFX,                              // clsid
    APO_FLAG_DEFAULT,                                    // flags
    L"StudioFeel System Equalizer",                     // szFriendlyName
    L"Copyright (c) StudioFeel Inc.",                   // szCopyrightInfo
    1,                                                   // u32MajorVersion
    0,                                                   // u32MinorVersion
    1,                                                   // u32MinInputConnections
    1,                                                   // u32MaxInputConnections
    1,                                                   // u32MinOutputConnections
    1,                                                   // u32MaxOutputConnections
    0,                                                   // u32MaxInstances (0 = unlimited)
    0,                                                   // u32NumAPOInterfaces
    nullptr                                              // iidAPOInterfaceList
};

// ============================================================================
// StudioFeelAPO Class
// ============================================================================

class __declspec(uuid("94B389EF-333E-4865-B0FC-0ABA99BCC7E9"))
StudioFeelAPO
    : public CBaseAudioProcessingObject
    , public IAudioSystemEffects2
{
public:
    StudioFeelAPO();
    virtual ~StudioFeelAPO();

    // ========================================================================
    // IUnknown
    // ========================================================================
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override;

    // ========================================================================
    // IAudioProcessingObject
    // ========================================================================

    // Called during initialization to set up the APO.
    // cbDataSize: size of pbyData
    // pbyData: pointer to APOInitSystemEffects2 or APOInitSystemEffects3 struct
    STDMETHOD(Initialize)(UINT32 cbDataSize, BYTE* pbyData) override;

    // Check if the proposed input format is supported.
    // The APO operates on float32 audio with 1-8 channels.
    STDMETHOD(IsInputFormatSupported)(
        IAudioMediaType* pOppositeFormat,
        IAudioMediaType* pRequestedInputFormat,
        IAudioMediaType** ppSupportedInputFormat) override;

    // Return APO registration properties
    STDMETHOD(GetRegistrationProperties)(APO_REG_PROPERTIES** ppRegProps) override;

    // ========================================================================
    // IAudioProcessingObjectConfiguration
    // ========================================================================

    // Called before real-time processing begins. Pre-allocate all buffers here.
    STDMETHOD(LockForProcess)(
        UINT32 u32NumInputConnections,
        APO_CONNECTION_DESCRIPTOR** ppInputConnections,
        UINT32 u32NumOutputConnections,
        APO_CONNECTION_DESCRIPTOR** ppOutputConnections) override;

    // Called after real-time processing ends. Free allocated resources.
    STDMETHOD(UnlockForProcess)() override;

    // ========================================================================
    // IAudioProcessingObjectRT
    // ========================================================================

    // Real-time audio processing callback.
    // CRITICAL: Must not allocate, lock, or make system calls.
    STDMETHOD_(void, APOProcess)(
        UINT32 u32NumInputConnections,
        APO_CONNECTION_PROPERTY** ppInputConnections,
        UINT32 u32NumOutputConnections,
        APO_CONNECTION_PROPERTY** ppOutputConnections) override;

    // ========================================================================
    // IAudioSystemEffects2
    // ========================================================================

    // Returns the APO's endpoint property store.
    STDMETHOD(GetEffectsList)(
        LPGUID* ppEffectsIds,
        UINT* pcEffects,
        HANDLE Event) override;

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    // Parse APOInit structure to extract endpoint ID and property store
    void ParseInitData(UINT32 cbDataSize, BYTE* pbyData);

    // Set up the named pipe server for IPC with the companion app
    void InitializeIPCServer();

    // Handle incoming IPC requests from the UI app
    std::string HandleIPCRequest(PipeMessageType type, const std::string& payload);

    // Process individual parameter updates (e.g., band.0.gain)
    std::string ProcessParameterUpdate(const std::string& key, const std::string& value);

    // Save current configuration to property store
    void SaveConfigurationToPropertyStore();

    // Load configuration from property store
    void LoadConfigurationFromPropertyStore();

    // ========================================================================
    // State
    // ========================================================================

    // Reference count for COM
    std::atomic<ULONG> m_refCount{1};

    // Audio format information (set during LockForProcess)
    uint32_t m_numChannels  = 0;
    uint32_t m_sampleRate   = 0;
    uint32_t m_bitsPerSample = 0;
    bool     m_isLocked     = false;

    // DSP engine (owns the filter chain)
    DSPEngine m_dspEngine;

    // Named pipe server for UI ↔ APO communication
    std::unique_ptr<NamedPipeServer> m_pipeServer;

    // Endpoint identification
    std::string m_endpointId;

    // Windows version (affects which init struct we receive)
    bool m_isWindows11 = false;

    // Property store pointer (from APOInit)
    IPropertyStore* m_propertyStore = nullptr;
};

// ============================================================================
// COM Class Factory
// ============================================================================

template <class T>
class CClassFactory : public IClassFactory {
public:
    // IUnknown
    STDMETHOD_(ULONG, AddRef)()  override { return InterlockedIncrement(&m_refCount); }
    STDMETHOD_(ULONG, Release)() override {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IClassFactory
    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
        if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;
        T* pObject = new (std::nothrow) T();
        if (!pObject) return E_OUTOFMEMORY;
        HRESULT hr = pObject->QueryInterface(riid, ppv);
        pObject->Release();
        return hr;
    }

    STDMETHOD(LockServer)(BOOL) override { return S_OK; }

    // Static helper to create a class factory instance
    static HRESULT CreateInstance(REFCLSID, REFIID riid, void** ppv) {
        CClassFactory* pFactory = new (std::nothrow) CClassFactory();
        if (!pFactory) return E_OUTOFMEMORY;
        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }

private:
    LONG m_refCount = 1;
};

} // namespace StudioFeel

#endif // _WIN32
