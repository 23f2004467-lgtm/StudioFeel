// ============================================================================
// StudioFeel — Main Page Code-Behind
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Shapes;
using System;
using System.Linq;

namespace StudioFeel
{
    /// <summary>
    /// The main page of the application containing the EQ controls.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainViewModel ViewModel { get; private set; } = null!;

        public MainPage()
        {
            ViewModel = new MainViewModel();
            this.InitializeComponent();
            DataContext = ViewModel;

            // Wire up the visualizer curve
            ViewModel.EQCurve = EQCurve;

            // Set the action that updates the curve
            ViewModel.SetUpdateCurveAction(UpdateCurve);

            // Subscribe to property changes to update curve
            ViewModel.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(MainViewModel.MasterGain))
                {
                    UpdateCurve();
                }
            };

            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
            SizeChanged += OnSizeChanged;
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            ViewModel.Start();
            UpdateCurve();
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            ViewModel.Stop();
        }

        private void OnSizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (e.NewSize.Width > 0)
            {
                UpdateCurve();
            }
        }

        private void UpdateCurve()
        {
            // Get the canvas actual width
            if (FrequencyCurveCanvas != null && FrequencyCurveCanvas.ActualWidth > 0)
            {
                ViewModel.UpdateCurve(
                    FrequencyCurveCanvas.ActualWidth,
                    FrequencyCurveCanvas.ActualHeight
                );
            }
        }

        private void OnDeviceSelected(object sender, RoutedEventArgs e)
        {
            if (sender is MenuFlyoutItem item && item.Tag is AudioDeviceInfo device)
            {
                ViewModel.SelectedDevice = device;
            }
        }
    }
}
