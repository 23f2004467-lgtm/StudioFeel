// ============================================================================
// StudioFeel — Named Pipe IPC Implementation
// ============================================================================
// Client side: The UI app uses this to talk to the APO driver.
// Server side: The APO driver uses NamedPipeServer to receive messages.
// ============================================================================

#include "../include/NamedPipeIPC.h"
#include "../include/EQParameters.h"

#ifdef _WIN32
#include <windows.h>
#include <sstream>
#include <thread>
#include <chrono>
#endif

namespace StudioFeel {

// ============================================================================
// Helper Functions
// ============================================================================

std::string NamedPipeIPC::MakePipeName(const std::string& endpointId) {
    // Create a safe pipe name from endpoint ID
    // Replace problematic characters with underscore
    std::string safeId = endpointId;
    for (char& c : safeId) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return "\\\\.\\pipe\\StudioFeel_" + safeId;
}

// ============================================================================
// NamedPipeIPC — Client Side (UI App)
// ============================================================================

NamedPipeIPC::NamedPipeIPC() = default;

NamedPipeIPC::~NamedPipeIPC() {
    Shutdown();
}

bool NamedPipeIPC::Initialize(const std::string& endpointId) {
    if (endpointId.empty()) return false;

    m_activeEndpointId = endpointId;
    std::string pipeName = MakePipeName(endpointId);

#ifdef _WIN32
    // Try to connect to the pipe (server is created by APO)
    // Wait up to 5 seconds for the pipe to be available
    for (int attempt = 0; attempt < 50; ++attempt) {
        m_pipeHandle = CreateFileA(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,                      // No sharing
            nullptr,                // Default security
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,   // Async I/O
            nullptr                 // No template
        );

        if (m_pipeHandle != INVALID_HANDLE_VALUE) {
            m_connected.store(true);
            return true;
        }

        DWORD error = GetLastError();
        if (error == ERROR_PIPE_BUSY) {
            // Wait for pipe to become available
            WaitNamedPipeA(pipeName.c_str(), 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Pipe doesn't exist yet (APO not loaded)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
#else
    // Non-Windows: can't connect
    (void)pipeName;
    return false;
#endif
}

bool NamedPipeIPC::IsConnected() const {
    return m_connected.load();
}

void NamedPipeIPC::Shutdown() {
    m_shutdownRequested.store(true);
    m_connected.store(false);

#ifdef _WIN32
    if (m_listenerThread.joinable()) {
        m_listenerThread.join();
    }
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }
#endif
}

bool NamedPipeIPC::GetConfiguration(EQConfiguration& outConfig) {
    PipeMessageType responseType;
    std::string responsePayload;

    if (!SendAndReceive(PipeMessageType::GetConfig, "", responseType, responsePayload)) {
        return false;
    }

    if (responseType == PipeMessageType::ConfigResponse) {
        return Json::DeserializeConfiguration(responsePayload, outConfig);
    }

    return false;
}

bool NamedPipeIPC::SetConfiguration(const EQConfiguration& config) {
    PipeMessageType responseType;
    std::string responsePayload;

    std::string payload = Json::SerializeConfiguration(config);

    if (!SendAndReceive(PipeMessageType::SetConfig, payload, responseType, responsePayload)) {
        return false;
    }

    return (responseType == PipeMessageType::AckResponse);
}

bool NamedPipeIPC::SetParameter(const std::string& parameterKey,
                                  const std::string& jsonValue) {
    PipeMessageType responseType;
    std::string responsePayload;

    // Build JSON payload: {"key": "band.0.gain", "value": 6.0}
    std::ostringstream oss;
    oss << "{\"key\":\"" << parameterKey << "\",\"value\":" << jsonValue << "}";

    if (!SendAndReceive(PipeMessageType::SetParameter, oss.str(), responseType, responsePayload)) {
        return false;
    }

    return (responseType == PipeMessageType::AckResponse);
}

bool NamedPipeIPC::GetAudioEndpoints(std::vector<AudioEndpointInfo>& outEndpoints) {
    // For now, return empty list
    // In a full implementation, this would enumerate Windows audio endpoints
    outEndpoints.clear();
    return true;
}

std::string NamedPipeIPC::GetActiveEndpointId() const {
    return m_activeEndpointId;
}

bool NamedPipeIPC::SetActiveEndpoint(const std::string& endpointId) {
    // Disconnect from current endpoint
    Shutdown();

    // Connect to new endpoint
    return Initialize(endpointId);
}

void NamedPipeIPC::OnParameterChanged(ParameterChangeCallback callback) {
    m_paramChangeCallback = std::move(callback);
}

void NamedPipeIPC::OnEndpointChanged(EndpointChangeCallback callback) {
    m_endpointChangeCallback = std::move(callback);
}

bool NamedPipeIPC::SendAndReceive(PipeMessageType requestType, const std::string& payload,
                                   PipeMessageType& outResponseType, std::string& outResponsePayload)
{
#ifdef _WIN32
    if (!IsConnected() || m_pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_pipeMutex);

    // Build message header
    PipeMessageHeader header;
    header.messageType = static_cast<uint32_t>(requestType);
    header.payloadSize = static_cast<uint32_t>(payload.size());

    // Send header
    DWORD bytesWritten = 0;
    BOOL success = WriteFile(
        m_pipeHandle,
        &header,
        sizeof(header),
        &bytesWritten,
        nullptr
    );

    if (!success || bytesWritten != sizeof(header)) {
        m_connected.store(false);
        return false;
    }

    // Send payload
    if (!payload.empty()) {
        bytesWritten = 0;
        success = WriteFile(
            m_pipeHandle,
            payload.data(),
            static_cast<DWORD>(payload.size()),
            &bytesWritten,
            nullptr
        );

        if (!success || bytesWritten != payload.size()) {
            m_connected.store(false);
            return false;
        }
    }

    // Read response header
    DWORD bytesRead = 0;
    success = ReadFile(
        m_pipeHandle,
        &header,
        sizeof(header),
        &bytesRead,
        nullptr
    );

    if (!success || bytesRead != sizeof(header)) {
        m_connected.store(false);
        return false;
    }

    outResponseType = static_cast<PipeMessageType>(header.messageType);

    // Read response payload
    if (header.payloadSize > 0) {
        outResponsePayload.resize(header.payloadSize);
        bytesRead = 0;
        success = ReadFile(
            m_pipeHandle,
            outResponsePayload.data(),
            header.payloadSize,
            &bytesRead,
            nullptr
        );

        if (!success || bytesRead != header.payloadSize) {
            m_connected.store(false);
            return false;
        }
    } else {
        outResponsePayload.clear();
    }

    return true;
#else
    (void)requestType;
    (void)payload;
    outResponseType = PipeMessageType::ErrorResponse;
    outResponsePayload = "{\"error\":\"Not implemented on this platform\"}";
    return false;
#endif
}

void NamedPipeIPC::NotificationListenerThread() {
    // Background thread to receive notifications from APO
    // (e.g., when another UI instance changes a parameter)
#ifdef _WIN32
    while (!m_shutdownRequested.load() && IsConnected()) {
        // Try to read a message with a small timeout
        // For now, this is a placeholder
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#endif
}

// ============================================================================
// NamedPipeServer — Server Side (APO Driver)
// ============================================================================

NamedPipeServer::NamedPipeServer() = default;

NamedPipeServer::~NamedPipeServer() {
    Stop();
}

bool NamedPipeServer::Start(const std::string& endpointId) {
    if (endpointId.empty()) return false;

    m_endpointId = endpointId;
    m_running.store(true);

    // Start the accept loop thread
    m_acceptThread = std::thread([this]() { AcceptLoop(); });

    return true;
}

void NamedPipeServer::Stop() {
    m_running.store(false);

#ifdef _WIN32
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }
#endif

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    // Close all client connections
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto client : m_connectedClients) {
#ifdef _WIN32
        if (client != INVALID_HANDLE_VALUE) {
            CloseHandle(client);
        }
#endif
        (void)client;  // Suppress warning on non-Windows
    }
    m_connectedClients.clear();
}

void NamedPipeServer::SetRequestHandler(RequestHandler handler) {
    m_requestHandler = std::move(handler);
}

void NamedPipeServer::BroadcastParameterChange(const std::string& key, const std::string& jsonValue) {
    // Send notification to all connected clients
    std::lock_guard<std::mutex> lock(m_clientsMutex);

    // Build notification message
    std::ostringstream oss;
    oss << "{\"key\":\"" << key << "\",\"value\":" << jsonValue << "}";

    std::string payload = oss.str();

    PipeMessageHeader header;
    header.messageType = static_cast<uint32_t>(PipeMessageType::ParamChanged);
    header.payloadSize = static_cast<uint32_t>(payload.size());

    for (auto client : m_connectedClients) {
#ifdef _WIN32
        if (client == INVALID_HANDLE_VALUE) continue;

        DWORD bytesWritten = 0;
        WriteFile(client, &header, sizeof(header), &bytesWritten, nullptr);
        WriteFile(client, payload.data(), static_cast<DWORD>(payload.size()), &bytesWritten, nullptr);
#endif
        (void)client;  // Suppress warning
    }
}

void NamedPipeServer::AcceptLoop() {
#ifdef _WIN32
    std::string pipeName = NamedPipeIPC::MakePipeName(m_endpointId);

    while (m_running.load()) {
        // Create the named pipe
        HANDLE hPipe = CreateNamedPipeA(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,         // Read/write access
            PIPE_TYPE_MESSAGE |         // Message type pipe
            PIPE_READMODE_MESSAGE |     // Message-read mode
            PIPE_WAIT,                  // Blocking mode
            PIPE_UNLIMITED_INSTANCES,   // Max instances
            MAX_PIPE_PAYLOAD,           // Output buffer size
            MAX_PIPE_PAYLOAD,           // Input buffer size
            0,                          // Default timeout
            nullptr                     // Default security
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            // Failed to create pipe, wait and retry
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // Wait for client to connect
        BOOL connected = ConnectNamedPipe(hPipe, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            continue;
        }

        // Add to clients list and handle in a separate thread
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            m_connectedClients.push_back(hPipe);
        }

        // Handle this client (for now, synchronous)
        // In production, spawn a thread per client
        HandleClient(hPipe);
    }
#endif
}

void NamedPipeServer::HandleClient(
#ifdef _WIN32
    HANDLE clientPipe
#else
    int clientFd
#endif
) {
#ifdef _WIN32
    while (m_running.load()) {
        // Read message header
        PipeMessageHeader header;
        DWORD bytesRead = 0;

        BOOL success = ReadFile(
            clientPipe,
            &header,
            sizeof(header),
            &bytesRead,
            nullptr
        );

        if (!success || bytesRead != sizeof(header)) {
            // Client disconnected or error
            break;
        }

        // Read payload
        std::string payload;
        if (header.payloadSize > 0) {
            payload.resize(header.payloadSize);
            bytesRead = 0;

            success = ReadFile(
                clientPipe,
                payload.data(),
                header.payloadSize,
                &bytesRead,
                nullptr
            );

            if (!success || bytesRead != header.payloadSize) {
                break;
            }
        }

        // Process the request
        std::string responsePayload = "{}";
        PipeMessageType responseType = PipeMessageType::AckResponse;

        if (m_requestHandler) {
            responsePayload = m_requestHandler(
                static_cast<PipeMessageType>(header.messageType),
                payload
            );

            // Check if response is an error
            if (responsePayload.find("\"error\"") != std::string::npos) {
                responseType = PipeMessageType::ErrorResponse;
            } else if (static_cast<uint32_t>(header.messageType) < 0x1000) {
                // Request messages get response counterparts
                responseType = static_cast<PipeMessageType>(
                    static_cast<uint32_t>(header.messageType) + 0x1000
                );
            }
        }

        // Send response
        PipeMessageHeader responseHeader;
        responseHeader.messageType = static_cast<uint32_t>(responseType);
        responseHeader.payloadSize = static_cast<uint32_t>(responsePayload.size());

        DWORD bytesWritten = 0;
        WriteFile(clientPipe, &responseHeader, sizeof(responseHeader), &bytesWritten, nullptr);
        if (!responsePayload.empty()) {
            WriteFile(clientPipe, responsePayload.data(), static_cast<DWORD>(responsePayload.size()), &bytesWritten, nullptr);
        }
    }

    // Remove from clients list
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        auto it = std::find(m_connectedClients.begin(), m_connectedClients.end(), clientPipe);
        if (it != m_connectedClients.end()) {
            m_connectedClients.erase(it);
        }
    }

    CloseHandle(clientPipe);
#else
    (void)clientFd;  // Suppress unused warning
#endif
}

} // namespace StudioFeel
