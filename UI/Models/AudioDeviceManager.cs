// ============================================================================
// StudioFeel — Audio Device Manager
// ============================================================================
// Enumerates and manages Windows audio output devices.
// Uses Windows Core Audio API (C++/WinRT interop).
// ============================================================================

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Media.Devices;

namespace StudioFeel
{
    /// <summary>
    /// Represents an audio output device.
    /// </summary>
    public class AudioDevice
    {
        public string Id { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        public bool IsDefault { get; set; }
    }

    /// <summary>
    /// Manages audio device enumeration and selection.
    /// </summary>
    public class AudioDeviceManager
    {
        /// <summary>
        /// Get all available audio output devices.
        /// </summary>
        public async Task<List<AudioDevice>> GetOutputDevicesAsync()
        {
            var devices = new List<AudioDevice>();

            // Get the default audio output device ID
            var defaultId = MediaDevice.GetDefaultAudioRenderId(AudioDeviceRole.Default);

            // Enumerate all audio output devices
            var selector = MediaDevice.GetAudioRenderSelector();
            var deviceInfo = await DeviceInformation.FindAllAsync(selector);

            foreach (var info in deviceInfo)
            {
                devices.Add(new AudioDevice
                {
                    Id = info.Id,
                    Name = info.Name,
                    IsDefault = info.Id == defaultId
                });
            }

            return devices;
        }

        /// <summary>
        /// Get the currently selected output device.
        /// </summary>
        public AudioDevice? GetCurrentDevice()
        {
            var defaultId = MediaDevice.GetDefaultAudioRenderId(AudioDeviceRole.Default);
            return new AudioDevice
            {
                Id = defaultId,
                Name = "Default Output",
                IsDefault = true
            };
        }
    }
}
