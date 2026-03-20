// ============================================================================
// StudioFeel — Value Converters
// ============================================================================
// Convert data values for display in the UI (e.g., bool → visibility)
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;
using System;
using Windows.UI;

namespace StudioFeel.Converters
{
    /// <summary>
    /// Converts bool to Visibility (true = Visible, false = Collapsed)
    /// </summary>
    public class BoolToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            return (value is bool boolValue && boolValue) ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            return value is Visibility visibility && visibility == Visibility.Visible;
        }
    }

    /// <summary>
    /// Inverts a boolean value
    /// </summary>
    public class InverseBoolConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            return !(value is bool boolValue && boolValue);
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            return !(value is bool boolValue && boolValue);
        }
    }

    /// <summary>
    /// Converts gain value to a brush color for visualization
    /// </summary>
    public class GainToBrushConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is double gain)
            {
                // Green for positive, red for negative, gray for zero
                if (gain > 0.5)
                    return new SolidColorBrush(ColorHelper.FromArgb(255, 76, 175, 80));
                else if (gain < -0.5)
                    return new SolidColorBrush(ColorHelper.FromArgb(255, 244, 67, 54));
                else
                    return new SolidColorBrush(ColorHelper.FromArgb(255, 158, 158, 158));
            }
            return new SolidColorBrush(ColorHelper.FromArgb(255, 158, 158, 158));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }

    /// <summary>
    /// Converts double to string with specified format
    /// </summary>
    public class DoubleFormatConverter : IValueConverter
    {
        public string Format { get; set; } = "F1";

        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is double d)
                return d.ToString(Format);
            if (value is float f)
                return f.ToString(Format);
            return value?.ToString() ?? "";
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (double.TryParse(value as string, out double result))
                return result;
            return 0.0;
        }
    }
}
