// ============================================================================
// StudioFeel — IPC Interface Implementation
// ============================================================================
// Factory function that creates the right IPC implementation based on Windows version.
// ============================================================================

#include "../include/IPCInterface.h"
#include "../include/NamedPipeIPC.h"

#ifdef _WIN32
#include <windows.h>
#include <versionhelpers.h>
#endif

namespace StudioFeel {

// ============================================================================
// OS Version Detection
// ============================================================================

WindowsVersion DetectWindowsVersion() {
#ifdef _WIN32
    // IsWindows10OrGreater returns true for Windows 10+ (including 11)
    if (!IsWindows10OrGreater()) {
        return WindowsVersion::Windows10;
    }

    // Windows 11 22H2+ has better CAPX support
    // We detect by checking build number
    // Win11 21H2 = build 22000, Win11 22H2 = build 22621
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
    DWORDLONG mask = 0;

    // Use RtlGetVersion for accurate version info
    typedef NTSTATUS (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW rovi = { sizeof(rovi) };
            if (pRtlGetVersion(&rovi) == 0) {
                if (rovi.dwMajorVersion == 10 && rovi.dwMinorVersion == 0 && rovi.dwBuildNumber >= 22621) {
                    return WindowsVersion::Windows11_22H2Plus;
                }
                if (rovi.dwMajorVersion == 10 && rovi.dwMinorVersion == 0 && rovi.dwBuildNumber >= 22000) {
                    return WindowsVersion::Windows11_Pre22H2;
                }
            }
        }
    }

    return WindowsVersion::Windows10;
#else
    // Non-Windows platforms (for future cross-platform support)
    return WindowsVersion::Windows10;
#endif
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<IPCInterface> IPCInterface::Create() {
    return std::make_unique<NamedPipeIPC>();
}

std::unique_ptr<IPCInterface> IPCInterface::Create(WindowsVersion osVersion) {
    // For now, only named pipe implementation exists
    // Future: add CAPX implementation for Windows 11 22H2+
    (void)osVersion;  // Suppress unused warning
    return std::make_unique<NamedPipeIPC>();
}

} // namespace StudioFeel
