#pragma once
// ============================================================================
// StudioFeel — IPC Interface (Abstract)
// ============================================================================
// Abstract interface for communication between the WinUI 3 companion app
// and the APO driver running in audiodg.exe.
//
// Two implementations:
//   - CAPXPropertyStoreIPC: Windows 11 22H2+ (recommended, uses CAPX framework)
//   - NamedPipeIPC:         Windows 10 / older Win11 (fallback)
//
// The UI app creates an IPC instance via IPCInterface::Create(), which
// auto-detects the OS version and returns the appropriate implementation.
// ============================================================================

#include "EQParameters.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace StudioFeel {

// ============================================================================
// OS Version Detection
// ============================================================================

enum class WindowsVersion {
    Windows10,              // Windows 10 (any build)
    Windows11_Pre22H2,      // Windows 11 before 22H2 (limited CAPX)
    Windows11_22H2Plus      // Windows 11 22H2+ (full CAPX support)
};

// Detect the running Windows version at runtime
WindowsVersion DetectWindowsVersion();

// ============================================================================
// IPC Callback Types
// ============================================================================

// Called when the APO notifies the UI of a parameter change
// (e.g., if another instance of the app changed a setting)
using ParameterChangeCallback = std::function<void(const std::string& parameterKey,
                                                     const std::string& jsonValue)>;

// Called when the active audio endpoint changes
using EndpointChangeCallback = std::function<void(const std::string& newEndpointId)>;

// ============================================================================
// IPC Interface
// ============================================================================

class IPCInterface {
public:
    virtual ~IPCInterface() = default;

    // ========================================================================
    // Connection Lifecycle
    // ========================================================================

    // Initialize the IPC connection for a specific audio endpoint.
    // endpointId: Windows audio endpoint ID string (from AudioEndpointInfo::id)
    // Returns true on success, false on failure.
    virtual bool Initialize(const std::string& endpointId) = 0;

    // Check if the IPC connection is active
    virtual bool IsConnected() const = 0;

    // Disconnect and release resources
    virtual void Shutdown() = 0;

    // ========================================================================
    // Configuration Read/Write
    // ========================================================================

    // Read the current EQ configuration from the APO.
    // Returns true on success.
    virtual bool GetConfiguration(EQConfiguration& outConfig) = 0;

    // Write a full EQ configuration to the APO.
    // The APO will apply this configuration on its next audio buffer.
    virtual bool SetConfiguration(const EQConfiguration& config) = 0;

    // Set a single parameter (optimized for real-time slider updates).
    // parameterKey: e.g., "band.0.gain", "master.enabled"
    // jsonValue: JSON-encoded value, e.g., "6.0", "true"
    virtual bool SetParameter(const std::string& parameterKey,
                              const std::string& jsonValue) = 0;

    // ========================================================================
    // Audio Endpoint Enumeration
    // ========================================================================

    // List all audio render endpoints on the system.
    virtual bool GetAudioEndpoints(std::vector<AudioEndpointInfo>& outEndpoints) = 0;

    // Get the currently active endpoint ID
    virtual std::string GetActiveEndpointId() const = 0;

    // Switch to a different audio endpoint.
    // This reconnects the IPC to the APO instance for the new endpoint.
    virtual bool SetActiveEndpoint(const std::string& endpointId) = 0;

    // ========================================================================
    // Change Notifications
    // ========================================================================

    // Register a callback for when the APO notifies of parameter changes.
    // Useful for multi-instance sync (two app windows open simultaneously).
    virtual void OnParameterChanged(ParameterChangeCallback callback) = 0;

    // Register a callback for audio endpoint changes (device plugged/unplugged).
    virtual void OnEndpointChanged(EndpointChangeCallback callback) = 0;

    // ========================================================================
    // Factory
    // ========================================================================

    // Create the appropriate IPC implementation for the current OS.
    // Returns a heap-allocated instance (caller owns the pointer).
    static std::unique_ptr<IPCInterface> Create();

    // Create a specific implementation (for testing)
    static std::unique_ptr<IPCInterface> Create(WindowsVersion osVersion);

protected:
    IPCInterface() = default;
};

} // namespace StudioFeel
