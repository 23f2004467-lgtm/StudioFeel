// ============================================================================
// StudioFeel — Audio Device Manager
// ============================================================================
// Enumerates audio output devices using Windows Core Audio APIs.
// Provides device ID and friendly name for each device.
// ============================================================================

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace StudioFeel
{
    /// <summary>
    /// Represents an audio output device.
    /// </summary>
    public class AudioDeviceInfo
    {
        public string Id { get; set; } = string.Empty;
        public string FriendlyName { get; set; } = string.Empty;
        public string EndpointId { get; set; } = string.Empty;  // For APO connection
    }

    /// <summary>
    /// Manages audio device enumeration using Windows MMDeviceAPI.
    /// </summary>
    public class AudioDeviceManager
    {
        // ========================================================================
        // Windows Audio API - Com Interop
        // ========================================================================

        private const int DEVICE_STATE_ACTIVE = 0x00000001;

        [ComImport]
        [Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
        private class MMDeviceEnumerator
        {
        }

        [ComImport]
        [Guid("A95664D2-9614-4F35-A746-DE8DB63617E6")]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        private interface IMMDeviceEnumerator
        {
            [PreserveSig]
            int GetDefaultAudioEndpoint(int dataFlow, int role, out IMMDevice ppDevice);
            // Other methods omitted
        }

        [ComImport]
        [Guid("D666063F-1587-4E43-81F1-B948E807363F")]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        private interface IMMDevice
        {
            [PreserveSig]
            int GetId([MarshalAs(UnmanagedType.LPWStr)] out string ppstrId);

            [PreserveSig]
            int OpenPropertyStore(int stgmAccess, out IPropertyStore ppProperties);
            // Other methods omitted
        }

        [ComImport]
        [Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99")]
        [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        private interface IPropertyStore
        {
            [PreserveSig]
            int GetCount(out int cProps);

            [PreserveSig]
            int GetAt(int iProp, out PropertyKey pkey);

            [PreserveSig]
            int GetValue(ref PropertyKey key, out PropVariant pvar);
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct PropertyKey
        {
            public Guid fmtid;
            public uint pid;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct PropVariant
        {
            public short vt;
            public ushort wReserved1;
            public ushort wReserved2;
            public ushort wReserved3;
            public IntPtr pwszVal;

            public void Clear()
            {
                if ((vt == 31) && (pwszVal != IntPtr.Zero))  // VT_LPWSTR
                {
                    Marshal.FreeCoTaskMem(pwszVal);
                    pwszVal = IntPtr.Zero;
                }
            }
        }

        private static readonly Guid PKEY_Device_FriendlyName = new Guid("A45C254E-DF1C-4EFD-8020-67D146A850E0");
        private const int PKEY_Device_FriendlyName_Pid = 14;

        // ========================================================================
        // Device Enumeration
        // ========================================================================

        /// <summary>
        /// Enumerates all active audio output devices.
        /// </summary>
        public async Task<List<AudioDeviceInfo>> GetOutputDevicesAsync()
        {
            return await Task.Run(() =>
            {
                var devices = new List<AudioDeviceInfo>();

                try
                {
                    var enumeratorType = typeof(MMDeviceEnumerator);
                    var enumerator = (IMMDeviceEnumerator)Activator.CreateInstance(enumeratorType);

                    // Enumerate all devices (requires IMMDeviceCollection, simplified approach)
                    // For now, get the default device and enumerate via activation

                    IMMDevice defaultDevice = null;
                    try
                    {
                        int hr = enumerator.GetDefaultAudioEndpoint(0, 0, out defaultDevice);  // eRender, eConsole
                        if (hr == 0 && defaultDevice != null)
                        {
                            var deviceInfo = GetDeviceInfo(defaultDevice);
                            if (deviceInfo != null)
                            {
                                devices.Add(deviceInfo);
                            }

                            // Note: Full enumeration would require IMMDeviceCollection
                            // For MVP, we at least get the default device
                        }
                    }
                    finally
                    {
                        if (defaultDevice != null)
                        {
                            Marshal.ReleaseComObject(defaultDevice);
                        }
                        Marshal.ReleaseComObject(enumerator);
                    }
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"Device enumeration failed: {ex.Message}");
                }

                return devices;
            });
        }

        /// <summary>
        /// Gets the default audio output device.
        /// </summary>
        public async Task<AudioDeviceInfo?> GetDefaultDeviceAsync()
        {
            return await Task.Run(() =>
            {
                try
                {
                    var enumeratorType = typeof(MMDeviceEnumerator);
                    var enumerator = (IMMDeviceEnumerator)Activator.CreateInstance(enumeratorType);

                    IMMDevice device = null;
                    try
                    {
                        int hr = enumerator.GetDefaultAudioEndpoint(0, 0, out device);
                        if (hr == 0 && device != null)
                        {
                            return GetDeviceInfo(device);
                        }
                    }
                    finally
                    {
                        if (device != null)
                        {
                            Marshal.ReleaseComObject(device);
                        }
                        Marshal.ReleaseComObject(enumerator);
                    }
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine($"Get default device failed: {ex.Message}");
                }

                return null;
            });
        }

        /// <summary>
        /// Extracts device info from an IMMDevice.
        /// </summary>
        private AudioDeviceInfo? GetDeviceInfo(IMMDevice device)
        {
            try
            {
                device.GetId(out string? deviceId);
                if (string.IsNullOrEmpty(deviceId))
                    return null;

                device.OpenPropertyStore(0, out IPropertyStore propStore);

                var key = new PropertyKey
                {
                    fmtid = PKEY_Device_FriendlyName,
                    pid = PKEY_Device_FriendlyName_Pid
                };

                propStore.GetValue(ref key, out PropVariant pv);
                propStore.GetCount(out int propCount);

                string friendlyName = "Unknown Device";
                if (pv.vt == 31)  // VT_LPWSTR
                {
                    friendlyName = Marshal.PtrToStringUni(pv.pwszVal) ?? "Unknown Device";
                }
                pv.Clear();

                Marshal.ReleaseComObject(propStore);

                return new AudioDeviceInfo
                {
                    Id = deviceId,
                    FriendlyName = friendlyName,
                    EndpointId = SanitizeEndpointId(deviceId)
                };
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"GetDeviceInfo failed: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// Converts Windows endpoint ID to a pipe-safe format.
        /// </summary>
        private string SanitizeEndpointId(string endpointId)
        {
            // Replace backslashes and other problematic characters with underscores
            return endpointId.Replace('\\', '_').Replace('{', '_').Replace('}', '_');
        }
    }
}
