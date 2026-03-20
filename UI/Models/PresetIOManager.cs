// ============================================================================
// StudioFeel — Preset Import/Export
// ============================================================================
// Handles importing and exporting .studiofeel preset files via file pickers.
// ============================================================================

using CommunityToolkit.Mvvm.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Windows.Storage;
using System;
using System.IO;
using System.Threading.Tasks;

namespace StudioFeel
{
    /// <summary>
    /// Manages preset import/export functionality.
    /// </summary>
    public class PresetIOManager
    {
        /// <summary>
        /// Export the current EQ configuration to a .studiofeel file.
        /// </summary>
        public async Task<bool> ExportPresetAsync(MainViewModel viewModel)
        {
            // Create file save picker
            var savePicker = new Windows.Storage.Pickers.FileSavePicker();

            // File type extensions
            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(App.MainWindow);
            WinRT.Interop.InitializeWithWindow.Initialize(savePicker, hwnd);

            savePicker.SuggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.DocumentsLibrary;
            savePicker.FileTypeChoices.Add("StudioFeel Preset", new[] { ".studiofeel" });
            savePicker.SuggestedFileName = "My EQ Preset";

            // Show picker
            Windows.Storage.StorageFile file = await savePicker.PickSaveFileAsync();
            if (file == null) return false;

            try
            {
                // Create preset JSON
                var preset = CreatePresetFromViewModel(viewModel);
                string json = SerializePreset(preset);

                // Write to file
                await Windows.Storage.FileIO.WriteTextAsync(file, json);
                return true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Export failed: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Import a .studiofeel file and apply its settings.
        /// </summary>
        public async Task<bool> ImportPresetAsync(MainViewModel viewModel)
        {
            // Create file open picker
            var openPicker = new Windows.Storage.Pickers.FileOpenPicker();

            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(App.MainWindow);
            WinRT.Interop.InitializeWithWindow.Initialize(openPicker, hwnd);

            openPicker.SuggestedStartLocation = Windows.Storage.Pickers.PickerLocationId.DocumentsLibrary;
            openPicker.FileTypeFilter.Add(".studiofeel");

            // Show picker
            Windows.Storage.StorageFile file = await openPicker.PickSingleFileAsync();
            if (file == null) return false;

            try
            {
                // Validate file size to prevent OOM attacks (max 1MB)
                var properties = await file.GetBasicPropertiesAsync();
                if (properties.Size > 1024 * 1024)
                {
                    System.Diagnostics.Debug.WriteLine("Preset file too large (> 1MB)");
                    return false;
                }

                // Read file
                string json = await Windows.Storage.FileIO.ReadTextAsync(file);

                // Parse and apply preset
                var config = ParsePresetJson(json);
                if (config != null)
                {
                    ApplyConfigurationToViewModel(viewModel, config);
                    return true;
                }
                return false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Import failed: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Save current settings as a named user preset.
        /// </summary>
        public async Task<bool> SaveUserPresetAsync(MainViewModel viewModel, string name)
        {
            // Simple save to LocalState
            var localFolder = Windows.Storage.ApplicationData.Current.LocalFolder;
            var presetFile = await localFolder.CreateFileAsync(
                $"{SanitizeFileName(name)}.studiofeel",
                Windows.Storage.CreationCollisionOption.ReplaceExisting
            );

            var preset = CreatePresetFromViewModel(viewModel);
            preset.name = name;
            preset.id = Guid.NewGuid().ToString();
            preset.category = "user";

            string json = SerializePreset(preset);
            await Windows.Storage.FileIO.WriteTextAsync(presetFile, json);

            return true;
        }

        // ========================================================================
        // Helpers
        // ========================================================================

        private IPC.Preset CreatePresetFromViewModel(MainViewModel viewModel)
        {
            var bands = new List<IPC.EQBandConfig>();
            foreach (var band in viewModel.Bands)
            {
                bands.Add(new IPC.EQBandConfig
                {
                    enabled = band.Enabled,
                    type = (IPC.FilterType)band.TypeIndex,
                    frequency = (float)band.Frequency,
                    Q = (float)band.Q,
                    gainDb = (float)band.Gain,
                    label = band.Label
                });
            }

            return new IPC.Preset
            {
                name = "Custom",
                description = "Custom EQ settings",
                category = "user",
                createdAt = CurrentTimestamp(),
                modifiedAt = CurrentTimestamp(),
                eqConfig = new IPC.EQConfiguration
                {
                    masterEnabled = viewModel.IsEQEnabled,
                    masterGainDb = (float)viewModel.MasterGain,
                    sampleRate = 48000,
                    bands = bands
                }
            };
        }

        private string SerializePreset(IPC.Preset preset)
        {
            return System.Text.Json.JsonSerializer.Serialize(preset, new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented = true
            });
        }

        private IPC.EQConfiguration? ParsePresetJson(string json)
        {
            try
            {
                using (var doc = System.Text.Json.JsonDocument.Parse(json))
                {
                    var root = doc.RootElement;
                    var eqConfig = root.GetProperty("eqConfig");

                    var config = new IPC.EQConfiguration
                    {
                        masterEnabled = root.GetProperty("eqConfig").GetProperty("masterEnabled").GetBoolean(),
                        masterGainDb = root.GetProperty("eqConfig").GetProperty("masterGainDb").GetSingle(),
                        sampleRate = root.GetProperty("eqConfig").GetProperty("sampleRate").GetUInt32(),
                        bands = new List<IPC.EQBandConfig>()
                    };

                    if (eqConfig.TryGetProperty("bands", out var bands))
                    {
                        foreach (var band in bands.EnumerateArray())
                        {
                            config.bands.Add(new IPC.EQBandConfig
                            {
                                enabled = band.GetProperty("enabled").GetBoolean(),
                                type = (IPC.FilterType)band.GetProperty("type").GetInt32(),
                                frequency = band.GetProperty("frequencyHz").GetSingle(),
                                Q = band.GetProperty("Q").GetSingle(),
                                gainDb = band.GetProperty("gainDb").GetSingle(),
                                label = band.TryGetProperty("label", out var label) ? label.GetString() ?? "" : ""
                            });
                        }
                    }

                    return config;
                }
            }
            catch
            {
                return null;
            }
        }

        private void ApplyConfigurationToViewModel(MainViewModel viewModel, IPC.EQConfiguration config)
        {
            viewModel.IsEQEnabled = config.masterEnabled;
            viewModel.MasterGain = config.masterGainDb;

            for (int i = 0; i < Math.Min(config.bands.Count, viewModel.Bands.Count); i++)
            {
                var band = config.bands[i];
                viewModel.Bands[i].Enabled = band.enabled;
                viewModel.Bands[i].TypeIndex = (int)band.type;
                viewModel.Bands[i].Frequency = band.frequency;
                viewModel.Bands[i].Q = band.Q;
                viewModel.Bands[i].Gain = band.gainDb;
            }
        }

        private ulong CurrentTimestamp()
        {
            return (ulong)(DateTimeOffset.UtcNow.ToUnixTimeSeconds());
        }

        private string SanitizeFileName(string name)
        {
            var invalid = new char[] { '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
            foreach (var c in invalid)
            {
                name = name.Replace(c, '_');
            }
            return name;
        }
    }
}
