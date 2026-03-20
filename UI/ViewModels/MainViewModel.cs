// ============================================================================
// StudioFeel — Main View Model
// ============================================================================
// The "brains" behind the UI. Handles IPC to APO, manages EQ state,
// loads/saves presets, and updates the frequency response visualizer.
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace StudioFeel
{
    /// <summary>
    /// Represents a single EQ band for display in the UI.
    /// </summary>
    public partial class BandViewModel : ObservableObject
    {
        private int _index;

        [ObservableProperty]
        private bool _enabled;

        [ObservableProperty]
        private string _label = "Band";

        [ObservableProperty]
        private int _typeIndex;

        [ObservableProperty]
        private string _typeDisplay = "Peaking";

        [ObservableProperty]
        private double _frequency = 1000.0;

        [ObservableProperty]
        private double _q = 1.0;

        [ObservableProperty]
        private double _gain = 0.0;

        [ObservableProperty]
        private string _gainDisplay = "0.0 dB";

        // Action to notify when band changes (for visualizer update)
        public Action? BandChanged { get; set; }

        partial void OnGainChanged(double value)
        {
            GainDisplay = $"{value:F1} dB";
            BandChanged?.Invoke();
        }

        partial void OnEnabledChanged(bool value)
        {
            BandChanged?.Invoke();
        }

        partial void OnFrequencyChanged(double value)
        {
            BandChanged?.Invoke();
        }

        partial void OnQChanged(double value)
        {
            BandChanged?.Invoke();
        }

        partial void OnTypeIndexChanged(int value)
        {
            TypeDisplay = value switch
            {
                0 => "Peaking",
                1 => "Low Shelf",
                2 => "High Shelf",
                3 => "Lowpass",
                4 => "Highpass",
                5 => "Notch",
                _ => "Peaking"
            };
            BandChanged?.Invoke();
        }
    }

    /// <summary>
    /// Main view model for the EQ page.
    /// </summary>
    public partial class MainViewModel : ObservableObject
    {
        // ========================================================================
        // State
        // ========================================================================

        private IPC.IPCInterface? _ipc;
        private IPC.PresetManager? _presetManager;
        private IPC.EQConfiguration _currentConfig;

        // ========================================================================
        // Observable Properties (bound to UI)
        // ========================================================================

        [ObservableProperty]
        private bool _isEQEnabled = true;

        [ObservableProperty]
        private double _masterGain = 0.0;

        [ObservableProperty]
        private string _masterGainDisplay = "0.0 dB";

        [ObservableProperty]
        private string _selectedDeviceName = "Default Output";

        public ObservableCollection<BandViewModel> Bands { get; } = new();

        // Visualizer reference (set by MainPage)
        public Microsoft.UI.Xaml.Shapes.Polyline? EQCurve { get; set; }

        private VisualizerViewModel _visualizer = new();

        // Action to trigger curve update
        private Action? _updateCurveAction;

        /// <summary>
        /// Updates the frequency response curve on the visualizer.
        /// Call this after any EQ change.
        /// </summary>
        public void UpdateCurve(double canvasWidth, double canvasHeight)
        {
            if (EQCurve != null)
            {
                _visualizer.UpdateCurve(EQCurve, Bands.ToList(), MasterGain, canvasWidth, canvasHeight);
            }
        }

        /// <summary>
        /// Request a curve update (called from bands when they change)
        /// </summary>
        public void RequestCurveUpdate()
        {
            _updateCurveAction?.Invoke();
        }

        /// <summary>
        /// Set the action that updates the curve (called by MainPage)
        /// </summary>
        public void SetUpdateCurveAction(Action action)
        {
            _updateCurveAction = action;
        }

        // ========================================================================
        // Constructor
        // ========================================================================

        public MainViewModel()
        {
            _currentConfig = new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0.0f,
                sampleRate = 48000,
                bands = new List<IPC.EQBandConfig>()
            };

            // Initialize with a default 10-band EQ
            InitializeDefaultBands();
        }

        // ========================================================================
        // Lifecycle
        // ========================================================================

        public void Start()
        {
            // Initialize IPC connection to APO
            Task.Run(async () => await InitializeIPCAsync());
        }

        public void Stop()
        {
            // Save current settings before exit
            _ipc?.Shutdown();
        }

        private async Task InitializeIPCAsync()
        {
            try
            {
                // TODO: Get actual endpoint ID from Windows audio API
                string endpointId = "default";

                _ipc = IPC.IPCInterface.Create();

                // Try to connect (may fail if APO not installed yet)
                bool connected = await Task.Run(() => _ipc.Initialize(endpointId));

                if (connected)
                {
                    // Load current configuration from APO
                    await LoadConfigurationAsync();
                }
            }
            catch (Exception ex)
            {
                // Log error but continue - app works in "demo mode" without APO
                System.Diagnostics.Debug.WriteLine($"IPC Init failed: {ex.Message}");
            }
        }

        // ========================================================================
        // Band Management
        // ========================================================================

        private void InitializeDefaultBands()
        {
            Bands.Clear();

            // Standard 10-band EQ frequencies
            double[] frequencies = { 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
            string[] labels = { "Sub", "Bass", "Low-Mid", "Mid", "Upper-Mid",
                                "Presence", "Upper", "High", "Ultra", "Air" };

            for (int i = 0; i < frequencies.Length; i++)
            {
                var band = new BandViewModel
                {
                    _index = i,
                    Enabled = true,
                    Label = labels[i],
                    TypeIndex = 0,  // Peaking
                    TypeDisplay = "Peaking",
                    Frequency = frequencies[i],
                    Q = 1.0,
                    Gain = 0.0,
                    GainDisplay = "0.0 dB"
                };
                // Wire up band changes to curve updates
                band.BandChanged = () => RequestCurveUpdate();
                Bands.Add(band);
            }
        }

        partial void OnMasterGainChanged(double value)
        {
            MasterGainDisplay = $"{value:F1} dB";

            // Send to APO
            Task.Run(async () =>
            {
                if (_ipc != null && _ipc.IsConnected())
                {
                    await Task.Run(() => _ipc.SetParameter("master.gain", value.ToString("F1")));
                }
            });
        }

        partial void OnIsEQEnabledChanged(bool value)
        {
            Task.Run(async () =>
            {
                if (_ipc != null && _ipc.IsConnected())
                {
                    await Task.Run(() => _ipc.SetParameter("master.enabled", value.ToString().ToLower()));
                }
            });
        }

        // ========================================================================
        // Configuration Loading/Saving
        // ========================================================================

        private async Task LoadConfigurationAsync()
        {
            if (_ipc == null || !_ipc.IsConnected()) return;

            try
            {
                IPC.EQConfiguration config;
                bool success = await Task.Run(() => _ipc.GetConfiguration(out config));

                if (success)
                {
                    _currentConfig = config;
                    ApplyConfigurationToUI(config);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Load config failed: {ex.Message}");
            }
        }

        private void ApplyConfigurationToUI(IPC.EQConfiguration config)
        {
            IsEQEnabled = config.masterEnabled;
            MasterGain = config.masterGainDb;

            // Update bands
            for (int i = 0; i < Math.Min(config.bands.Count, Bands.Count); i++)
            {
                var band = config.bands[i];
                if (i < Bands.Count)
                {
                    Bands[i].Enabled = band.enabled;
                    Bands[i].Frequency = band.frequency;
                    Bands[i].Q = band.Q;
                    Bands[i].Gain = band.gainDb;
                    Bands[i].TypeIndex = (int)band.type;
                }
            }
        }

        // ========================================================================
        // Preset Loading
        // ========================================================================

        private async Task LoadPresetAsync(string presetId)
        {
            IPC.EQConfiguration config = presetId switch
            {
                "flat" => CreateFlatConfig(),
                "bass_boost" => CreateBassBoostConfig(),
                "treble_boost" => CreateTrebleBoostConfig(),
                "voice_clarity" => CreateVoiceClarityConfig(),
                "pop" => CreatePopConfig(),
                "rock" => CreateRockConfig(),
                "classical" => CreateClassicalConfig(),
                "gaming" => CreateGamingConfig(),
                "cinema" => CreateCinemaConfig(),
                "podcast" => CreatePodcastConfig(),
                _ => CreateFlatConfig()
            };

            _currentConfig = config;
            ApplyConfigurationToUI(config);

            // Send to APO
            if (_ipc != null && _ipc.IsConnected())
            {
                await Task.Run(() => _ipc.SetConfiguration(config));
            }
        }

        private IPC.EQConfiguration CreateFlatConfig()
        {
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select(b => new IPC.EQBandConfig
                {
                    enabled = b.Enabled,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = 0
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreateBassBoostConfig()
        {
            var bands = new List<IPC.EQBandConfig>();
            for (int i = 0; i < Bands.Count; i++)
            {
                bands.Add(new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)Bands[i].Frequency,
                    Q = (float)Bands[i].Q,
                    gainDb = (i < 2) ? 6.0f : 0.0f  // Boost sub and bass
                });
            }
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = bands
            };
        }

        private IPC.EQConfiguration CreateTrebleBoostConfig()
        {
            var bands = new List<IPC.EQBandConfig>();
            for (int i = 0; i < Bands.Count; i++)
            {
                bands.Add(new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)Bands[i].Frequency,
                    Q = (float)Bands[i].Q,
                    gainDb = (i >= 7) ? 5.0f : 0.0f  // Boost high frequencies
                });
            }
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = bands
            };
        }

        private IPC.EQConfiguration CreateVoiceClarityConfig()
        {
            // Boost presence (2-4kHz) and brightness (6kHz)
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i == 5 || i == 6) ? 4.0f : 0.0f
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreatePopConfig()
        {
            // Bass + slight upper-mid + treble boost
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i < 2) ? 4.0f : (i == 5) ? 2.0f : (i >= 8) ? 3.0f : 0.0f
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreateRockConfig()
        {
            // Punchy bass + mid presence
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i == 0) ? 5.0f : (i == 3) ? -2.0f : (i == 5) ? 3.0f : (i >= 7) ? 2.0f : 0.0f
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreateClassicalConfig()
        {
            // Subtle, balanced response
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i == 0) ? 2.0f : (i == 4) ? 1.0f : (i >= 8) ? 2.0f : 0.0f
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreateGamingConfig()
        {
            // Footsteps (2kHz) + detail (6kHz) + explosions
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i == 0) ? 5.0f : (i == 5) ? 4.0f : (i >= 6) ? 3.0f : 0.0f
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreateCinemaConfig()
        {
            // Deep bass + dialogue clarity
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = i == 0 ? IPC.FilterType.LowShelf : IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i == 0) ? 6.0f : (i == 5) ? 3.0f : 0.0f
                }).ToList()
            };
        }

        private IPC.EQConfiguration CreatePodcastConfig()
        {
            // Focus on speech (3kHz)
            return new IPC.EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = Bands.Select((b, i) => new IPC.EQBandConfig
                {
                    enabled = true,
                    type = IPC.FilterType.Peaking,
                    frequency = (float)b.Frequency,
                    Q = (float)b.Q,
                    gainDb = (i == 5) ? 5.0f : (i == 1 || i == 2) ? -2.0f : 0.0f
                }).ToList()
            };
        }

        // ========================================================================
        // Commands (button clicks)
        // ========================================================================

        [RelayCommand]
        private void ResetToFlat()
        {
            foreach (var band in Bands)
            {
                band.Gain = 0.0;
            }
            MasterGain = 0.0;
            IsEQEnabled = true;
        }

        // Preset loaders
        [RelayCommand]
        private void LoadPresetFlat() => Task.Run(() => LoadPresetAsync("flat"));

        [RelayCommand]
        private void LoadPresetBassBoost() => Task.Run(() => LoadPresetAsync("bass_boost"));

        [RelayCommand]
        private void LoadPresetTrebleBoost() => Task.Run(() => LoadPresetAsync("treble_boost"));

        [RelayCommand]
        private void LoadPresetVoiceClarity() => Task.Run(() => LoadPresetAsync("voice_clarity"));

        [RelayCommand]
        private void LoadPresetPop() => Task.Run(() => LoadPresetAsync("pop"));

        [RelayCommand]
        private void LoadPresetRock() => Task.Run(() => LoadPresetAsync("rock"));

        [RelayCommand]
        private void LoadPresetClassical() => Task.Run(() => LoadPresetAsync("classical"));

        [RelayCommand]
        private void LoadPresetGaming() => Task.Run(() => LoadPresetAsync("gaming"));

        [RelayCommand]
        private void LoadPresetCinema() => Task.Run(() => LoadPresetAsync("cinema"));

        [RelayCommand]
        private void LoadPresetPodcast() => Task.Run(() => LoadPresetAsync("podcast"));
    }
}
