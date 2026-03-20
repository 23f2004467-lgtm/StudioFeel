#pragma once
// ============================================================================
// StudioFeel — Named Pipe IPC (Windows 10 Fallback)
// ============================================================================
// Communication between the WinUI 3 app and the APO via named pipes.
// The APO creates the pipe server in its Initialize(); the UI app connects
// as a client.
//
// Pipe name format: \\.\pipe\StudioFeel_{EndpointId}
//
// Message protocol:
//   [4 bytes] MessageType (uint32_t)
//   [4 bytes] PayloadSize (uint32_t)
//   [N bytes] JSON payload
//
// Security: Pipe has DACL granting access to SYSTEM, Administrators, and Users.
// ============================================================================

#include "IPCInterface.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <thread>
#include <mutex>
#include <atomic>
#include <string>

namespace StudioFeel {

// ============================================================================
// Message Types
// ============================================================================

enum class PipeMessageType : uint32_t {
    // UI → APO requests
    GetConfig       = 0x0001,   // Request current config (no payload)
    SetConfig       = 0x0002,   // Set full config (payload: JSON EQConfiguration)
    SetParameter    = 0x0003,   // Set single param (payload: JSON {key, value})
    GetEndpoints    = 0x0004,   // Request endpoint list (no payload)
    SetEndpoint     = 0x0005,   // Switch endpoint (payload: endpoint ID string)

    // APO → UI responses/notifications
    ConfigResponse  = 0x1001,   // Response to GetConfig (payload: JSON EQConfiguration)
    AckResponse     = 0x1002,   // Success acknowledgement (no payload)
    ErrorResponse   = 0x1003,   // Error (payload: error message string)
    EndpointList    = 0x1004,   // Response to GetEndpoints (payload: JSON array)
    ParamChanged    = 0x2001,   // Notification: param changed (payload: JSON {key, value})
    EndpointChanged = 0x2002    // Notification: endpoint changed (payload: endpoint ID)
};

// ============================================================================
// Pipe Message Header
// ============================================================================

#pragma pack(push, 1)
struct PipeMessageHeader {
    uint32_t messageType;
    uint32_t payloadSize;
};
#pragma pack(pop)

static constexpr uint32_t MAX_PIPE_PAYLOAD = 16384;  // 16 KB max per message

// ============================================================================
// Named Pipe IPC Implementation (Client Side — for UI app)
// ============================================================================

class NamedPipeIPC : public IPCInterface {
public:
    NamedPipeIPC();
    ~NamedPipeIPC() override;

    // IPCInterface implementation
    bool Initialize(const std::string& endpointId) override;
    bool IsConnected() const override;
    void Shutdown() override;

    bool GetConfiguration(EQConfiguration& outConfig) override;
    bool SetConfiguration(const EQConfiguration& config) override;
    bool SetParameter(const std::string& parameterKey,
                      const std::string& jsonValue) override;

    bool GetAudioEndpoints(std::vector<AudioEndpointInfo>& outEndpoints) override;
    std::string GetActiveEndpointId() const override;
    bool SetActiveEndpoint(const std::string& endpointId) override;

    void OnParameterChanged(ParameterChangeCallback callback) override;
    void OnEndpointChanged(EndpointChangeCallback callback) override;

    // ========================================================================
    // Server-Side API (called by APO)
    // ========================================================================

    // Create a named pipe server for the given endpoint.
    // Called from StudioFeelAPO::Initialize().
    static bool CreatePipeServer(const std::string& endpointId);

    // Destroy the pipe server (called from APO teardown)
    static void DestroyPipeServer(const std::string& endpointId);

private:
    // Build pipe name from endpoint ID
    static std::string MakePipeName(const std::string& endpointId);

    // Send a message and wait for response (blocking, with timeout)
    bool SendAndReceive(PipeMessageType requestType, const std::string& payload,
                        PipeMessageType& outResponseType, std::string& outResponsePayload);

    // Background thread for receiving APO → UI notifications
    void NotificationListenerThread();

    // Connection state
#ifdef _WIN32
    HANDLE m_pipeHandle = INVALID_HANDLE_VALUE;
#endif
    std::string m_activeEndpointId;
    std::atomic<bool> m_connected{false};

    // Notification listener
    std::thread m_listenerThread;
    std::atomic<bool> m_shutdownRequested{false};

    // Callbacks
    ParameterChangeCallback m_paramChangeCallback;
    EndpointChangeCallback  m_endpointChangeCallback;

    // Serialization mutex (pipe is not thread-safe for concurrent writes)
    std::mutex m_pipeMutex;
};

// ============================================================================
// Named Pipe Server (APO side)
// ============================================================================
// The APO creates this in its Initialize() method. It listens for connections
// from the UI app and dispatches messages to the DSP engine.
// ============================================================================

class NamedPipeServer {
public:
    NamedPipeServer();
    ~NamedPipeServer();

    // Start listening for client connections
    bool Start(const std::string& endpointId);

    // Stop the server and close all connections
    void Stop();

    // Called by APO to broadcast parameter changes to connected clients
    void BroadcastParameterChange(const std::string& key, const std::string& jsonValue);

    // Message handler type: receives a request and returns a response payload
    using RequestHandler = std::function<std::string(PipeMessageType type,
                                                      const std::string& payload)>;
    void SetRequestHandler(RequestHandler handler);

private:
    void AcceptLoop();
    void HandleClient(
#ifdef _WIN32
        HANDLE clientPipe
#else
        int clientFd
#endif
    );

#ifdef _WIN32
    HANDLE m_pipeHandle = INVALID_HANDLE_VALUE;
#endif
    std::string m_endpointId;
    std::thread m_acceptThread;
    std::atomic<bool> m_running{false};
    RequestHandler m_requestHandler;
    std::mutex m_clientsMutex;
    std::vector<
#ifdef _WIN32
        HANDLE
#else
        int
#endif
    > m_connectedClients;
};

} // namespace StudioFeel
