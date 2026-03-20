// ============================================================================
// StudioFeel — Main Page Code-Behind
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace StudioFeel
{
    /// <summary>
    /// The main page of the application containing the EQ controls.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainViewModel ViewModel { get; }

        public MainPage()
        {
            this.InitializeComponent();
            ViewModel = new MainViewModel();
            DataContext = ViewModel;

            // Initialize the visualizer
            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            ViewModel.Start();
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            ViewModel.Stop();
        }
    }
}
