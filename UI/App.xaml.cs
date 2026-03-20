// ============================================================================
// StudioFeel — App Entry Point
// ============================================================================
// Main application class. Initializes the window, services, and theme.
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;

namespace StudioFeel
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    public partial class App : Application
    {
        private Window? _window;

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
            _window = new Window();
            _window.Title = "StudioFeel";

            // Create and navigate to the main page
            _window.Content = new Frame();
            ((Frame)_window.Content).Navigate(typeof(MainPage));

            _window.Activate();
        }

        private void OnActivated(object? sender, AppActivatedEventArgs e)
        {
            // Handle file activation (.studiofeel preset files)
            if (e.Kind == ActivationKind.File)
            {
                var fileArgs = (FileActivatedEventArgs)e;
                // TODO: Import the preset file
            }

            _window ??= new Window();
            _window.Activate();
        }
    }
}
