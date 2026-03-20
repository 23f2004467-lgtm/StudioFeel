// ============================================================================
// StudioFeel — App Entry Point
// ============================================================================
// Main application class. Initializes the window, services, and theme.
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.IO;
using System.Threading.Tasks;

namespace StudioFeel
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    public partial class App : Application
    {
        public static Window? MainWindow { get; private set; }

        private static MainPage? _mainPage;
        private static string? _pendingPresetFile;

        public App()
        {
            InitializeComponent();

            // Handle activation (e.g., when opening .studiofeel files)
            Activated += OnActivated;
        }

        /// <summary>
        /// Invoked when the application is launched normally by the end user.
        /// </summary>
        protected override void OnLaunched(LaunchActivatedEventArgs args)
        {
            InitializeMainWindow();
        }

        private void InitializeMainWindow()
        {
            MainWindow = new Window();
            MainWindow.Title = "StudioFeel";

            // Create and navigate to the main page
            MainWindow.Content = new Frame();
            _mainPage = new MainPage();
            ((Frame)MainWindow.Content).Content = _mainPage;

            MainWindow.Activate();

            // If there's a pending preset file to load, apply it after UI is ready
            if (!string.IsNullOrEmpty(_pendingPresetFile))
            {
                _ = Task.Delay(100).ContinueWith(_ =>
                {
                    MainWindow.DispatcherQueue.TryEnqueue(() =>
                    {
                        LoadPresetFromFile(_pendingPresetFile);
                        _pendingPresetFile = null;
                    });
                });
            }
        }

        private void OnActivated(object? sender, AppActivatedEventArgs e)
        {
            if (e.Kind == ActivationKind.File)
            {
                var fileArgs = (FileActivatedEventArgs)e;

                if (fileArgs.Files.Count > 0)
                {
                    var file = fileArgs.Files[0].Path;
                    if (Path.GetExtension(file).Equals(".studiofeel", StringComparison.OrdinalIgnoreCase))
                    {
                        if (_mainPage != null)
                        {
                            // App already running, load immediately
                            LoadPresetFromFile(file);
                        }
                        else
                        {
                            // App still initializing, store for later
                            _pendingPresetFile = file;
                            InitializeMainWindow();
                            return;
                        }
                    }
                }
            }

            // Ensure window exists and is activated
            if (MainWindow == null)
            {
                InitializeMainWindow();
            }
            else
            {
                MainWindow.Activate();
            }
        }

        private void LoadPresetFromFile(string filePath)
        {
            try
            {
                // Validate file size to prevent OOM attacks (max 1MB)
                var fileInfo = new FileInfo(filePath);
                if (fileInfo.Length > 1024 * 1024)
                {
                    System.Diagnostics.Debug.WriteLine("Preset file too large (> 1MB)");
                    return;
                }

                string json = File.ReadAllText(filePath);
                var config = ParsePresetJson(json);

                if (config != null && _mainPage?.ViewModel != null)
                {
                    ApplyConfigurationToViewModel(_mainPage.ViewModel, config);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to load preset: {ex.Message}");
            }
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
                        masterEnabled = eqConfig.GetProperty("masterEnabled").GetBoolean(),
                        masterGainDb = eqConfig.GetProperty("masterGainDb").GetSingle(),
                        sampleRate = eqConfig.GetProperty("sampleRate").GetUInt32(),
                        bands = new System.Collections.Generic.List<IPC.EQBandConfig>()
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
    }
}
