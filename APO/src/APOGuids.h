#pragma once
// ============================================================================
// StudioFeel APO - GUID Definitions
// ============================================================================
// All CLSIDs and IIDs used by the StudioFeel Audio Processing Object.
// Generated once; do NOT regenerate — these are baked into the INF and registry.
// ============================================================================

#include <guiddef.h>

// {94B389EF-333E-4865-B0FC-0ABA99BCC7E9}
// Main EFX APO — the endpoint-level equalizer loaded into audiodg.exe
DEFINE_GUID(CLSID_StudioFeel_EFX,
    0x94B389EF, 0x333E, 0x4865,
    0xB0, 0xFC, 0x0A, 0xBA, 0x99, 0xBC, 0xC7, 0xE9);

// {3CEE3D8C-91A0-4B8A-9AC6-0F30DDF11923}
// FX Property Store handler — exposes CAPX settings to the HSA companion app
DEFINE_GUID(CLSID_StudioFeel_PropertyStore,
    0x3CEE3D8C, 0x91A0, 0x4B8A,
    0x9A, 0xC6, 0x0F, 0x30, 0xDD, 0xF1, 0x19, 0x23);

// {6FE20C8C-1313-41D1-AE51-AF5EAF00C607}
// Audio system effects identifier — tells Windows this DLL is an APO
DEFINE_GUID(IID_StudioFeel_AudioSystemEffects,
    0x6FE20C8C, 0x1313, 0x41D1,
    0xAE, 0x51, 0xAF, 0x5E, 0xAF, 0x00, 0xC6, 0x07);

// {108D9DF5-A690-4912-8437-1A4B0B81C989}
// Property effect CLSID — used in INF FxProperties registration
DEFINE_GUID(CLSID_StudioFeel_PropertyEffect,
    0x108D9DF5, 0xA690, 0x4912,
    0x84, 0x37, 0x1A, 0x4B, 0x0B, 0x81, 0xC9, 0x89);

// {E6459A10-48D8-4098-9BD0-9FEB6820BA3B}
// Custom property key category for StudioFeel EQ settings in the property store
DEFINE_GUID(STUDIOFEEL_PROPERTYKEY_CATEGORY,
    0xE6459A10, 0x48D8, 0x4098,
    0x9B, 0xD0, 0x9F, 0xEB, 0x68, 0x20, 0xBA, 0x3B);

// {2711999A-0927-4ADB-9203-68AFFAEE16CB}
// Class factory CLSID (internal use for DllGetClassObject routing)
DEFINE_GUID(CLSID_StudioFeel_ClassFactory,
    0x2711999A, 0x0927, 0x4ADB,
    0x92, 0x03, 0x68, 0xAF, 0xFA, 0xEE, 0x16, 0xCB);

// ============================================================================
// Property Key IDs (PIDs) — used with STUDIOFEEL_PROPERTYKEY_CATEGORY
// ============================================================================
// These combine with the category GUID above to form PROPERTYKEY structs
// for reading/writing individual EQ parameters in the CAPX property store.
// ============================================================================

constexpr DWORD PID_STUDIOFEEL_MASTER_ENABLED   = 1;
constexpr DWORD PID_STUDIOFEEL_MASTER_GAIN      = 2;
constexpr DWORD PID_STUDIOFEEL_BAND_COUNT       = 3;
constexpr DWORD PID_STUDIOFEEL_BAND_CONFIG_BASE = 100;  // Band N starts at 100 + N*10
// Per band: +0 = type, +1 = frequency, +2 = Q, +3 = gain, +4 = enabled

// Helper to get property key for a specific band parameter
inline PROPERTYKEY MakeStudioFeelBandKey(int bandIndex, int paramOffset) {
    PROPERTYKEY key;
    key.fmtid = STUDIOFEEL_PROPERTYKEY_CATEGORY;
    key.pid = PID_STUDIOFEEL_BAND_CONFIG_BASE + (bandIndex * 10) + paramOffset;
    return key;
}

// Band parameter offsets
constexpr int BAND_PARAM_TYPE      = 0;
constexpr int BAND_PARAM_FREQUENCY = 1;
constexpr int BAND_PARAM_Q         = 2;
constexpr int BAND_PARAM_GAIN      = 3;
constexpr int BAND_PARAM_ENABLED   = 4;
