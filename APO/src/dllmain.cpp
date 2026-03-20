// ============================================================================
// StudioFeel — APO DLL Entry Point and COM Registration
// ============================================================================
// This is the entry point for the StudioFeel.APO.dll COM server.
// The DLL is loaded by audiodg.exe when the APO is registered via the INF.
//
// Exports:
//   DllGetClassObject — Returns class factories for our COM objects
//   DllCanUnloadNow   — Indicates whether the DLL can be safely unloaded
//   DllRegisterServer  / DllUnregisterServer — Self-registration (optional)
// ============================================================================

#ifdef _WIN32

#include <windows.h>
#include <combaseapi.h>

#include "APOGuids.h"
#include "StudioFeelAPO.h"

using namespace StudioFeel;

// ============================================================================
// Globals
// ============================================================================

HINSTANCE g_hInstance = nullptr;
static volatile LONG g_moduleRefCount = 0;

void DllAddRef()  { InterlockedIncrement(&g_moduleRefCount); }
void DllRelease() { InterlockedDecrement(&g_moduleRefCount); }

// ============================================================================
// DllMain
// ============================================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_hInstance = hModule;
            DisableThreadLibraryCalls(hModule);
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

// ============================================================================
// DllGetClassObject
// ============================================================================
// Called by COM when a client requests an object with one of our CLSIDs.
// We return the appropriate class factory.
// ============================================================================

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    // Main EFX APO
    if (IsEqualCLSID(rclsid, CLSID_StudioFeel_EFX)) {
        return CClassFactory<StudioFeelAPO>::CreateInstance(rclsid, riid, ppv);
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

// ============================================================================
// DllCanUnloadNow
// ============================================================================
// Returns S_OK if no objects are still alive and the DLL can be unloaded.
// ============================================================================

STDAPI DllCanUnloadNow()
{
    return (g_moduleRefCount == 0) ? S_OK : S_FALSE;
}

// ============================================================================
// DllRegisterServer / DllUnregisterServer
// ============================================================================
// Optional self-registration (not typically used for APOs since registration
// is handled by the INF file, but included for completeness).
// ============================================================================

STDAPI DllRegisterServer()
{
    // Registration is handled entirely by the INF file.
    // This export exists only because some COM infrastructure expects it.
    return S_OK;
}

STDAPI DllUnregisterServer()
{
    return S_OK;
}

#endif // _WIN32
