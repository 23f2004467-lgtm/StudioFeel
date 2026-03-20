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
        public static Window? MainWindow { get; private set; }

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
            MainWindow = new Window();
            MainWindow.Title = "StudioFeel";

            // Create and navigate to the main page
            MainWindow.Content = new Frame();
            ((Frame)MainWindow.Content).Navigate(typeof(MainPage));

            MainWindow.Activate();
        }

        private void OnActivated(object? sender, AppActivatedEventArgs e)
        {
            // Handle file activation (.studiofeel preset files)
            if (e.Kind == ActivationKind.File)
            {
                var fileArgs = (FileActivatedEventArgs)e;
                // TODO: Import the preset file
            }

            MainWindow ??= new Window();
            MainWindow.Activate();
        }
    }
}
