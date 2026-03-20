// ============================================================================
// StudioFeel — Frequency Response Visualizer
// ============================================================================
// Draws the EQ curve on the canvas. Shows how each filter affects
// the frequency response from 20Hz to 20kHz.
// ============================================================================

using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Shapes;
using System;
using System.Collections.Generic;

namespace StudioFeel
{
    /// <summary>
    /// Handles drawing the frequency response curve on the Canvas.
    /// </summary>
    public class VisualizerViewModel
    {
        private const int MinFrequency = 20;
        private const int MaxFrequency = 20000;
        private const int CurvePoints = 100;

        /// <summary>
        /// Updates the frequency curve polyline based on current EQ settings.
        /// </summary>
        public void UpdateCurve(
            Polyline curve,
            List<BandViewModel>? bands,
            double masterGain,
            double canvasWidth,
            double canvasHeight)
        {
            if (curve == null || bands == null) return;
            if (canvasWidth <= 0 || canvasHeight <= 0) return;

            // Calculate frequency response at log-spaced points
            var points = new PointCollection();

            double logMin = Math.Log(MinFrequency);
            double logMax = Math.Log(MaxFrequency);
            double logRange = logMax - logMin;

            // Y scale: +/- 12dB maps to canvas height
            double centerY = canvasHeight / 2;
            double pixelsPerDb = (canvasHeight / 2) / 12.0;

            for (int i = 0; i <= CurvePoints; i++)
            {
                double t = (double)i / CurvePoints;
                double logFreq = logMin + t * logRange;
                double frequency = Math.Exp(logFreq);

                // Calculate total gain at this frequency
                double totalDb = masterGain;

                // Add contribution from each enabled band
                for (int b = 0; b < bands.Count; b++)
                {
                    if (!bands[b].Enabled) continue;

                    totalDb += GetBandResponseAtFrequency(
                        bands[b],
                        frequency,
                        48000  // Assume 48kHz sample rate
                    );
                }

                // Clamp to display range
                totalDb = Math.Max(-12, Math.Min(12, totalDb));

                // Convert to canvas coordinates
                double x = t * canvasWidth;
                double y = centerY - (totalDb * pixelsPerDb);

                points.Add(new Windows.Foundation.Point(x, y));
            }

            curve.Points = points;
        }

        /// <summary>
        /// Calculate the gain (in dB) of a single band at a given frequency.
        /// Uses the Audio EQ Cookbook formulas for biquad filters.
        /// </summary>
        private double GetBandResponseAtFrequency(
            BandViewModel band,
            double frequency,
            double sampleRate)
        {
            double A = Math.Pow(10, band.Gain / 40.0);
            double omega = 2 * Math.PI * frequency / sampleRate;
            double sinW = Math.Sin(omega);
            double cosW = Math.Cos(omega);

            double magnitudeDb = 0;

            switch (band.TypeIndex)
            {
                case 0: // Peaking
                    {
                        double alpha = sinW / (2 * band.Q);
                        double num = Math.Sqrt(
                            Math.Pow(1 + alpha * A, 2) +
                            Math.Pow(-2 * cosW, 2)
                        );
                        double den = Math.Sqrt(
                            Math.Pow(1 + alpha / A, 2) +
                            Math.Pow(-2 * cosW, 2)
                        );
                        magnitudeDb = 20 * Math.Log10(num / den);
                    }
                    break;

                case 1: // Low Shelf
                    {
                        double S = 1.0;
                        double alpha = sinW / 2 * Math.Sqrt(
                            (A + 1 / A) * (1 / S - 1) + 2
                        );
                        double sqrtA2alpha = 2 * Math.Sqrt(A) * alpha;

                        double numReal = A * ((A + 1) - (A - 1) * cosW + sqrtA2alpha);
                        double numImag = A * 2 * Math.Sqrt(Math.Max(0, (A - 1) - (A + 1) * cosW));
                        double num = Math.Sqrt(numReal * numReal + numImag * numImag);

                        double denReal = (A + 1) + (A - 1) * cosW + sqrtA2alpha;
                        double denImag = 2 * Math.Sqrt(Math.Max(0, (A - 1) + (A + 1) * cosW));
                        double den = Math.Sqrt(denReal * denReal + denImag * denImag);

                        magnitudeDb = 20 * Math.Log10(num / den);
                    }
                    break;

                case 2: // High Shelf
                    {
                        double S = 1.0;
                        double alpha = sinW / 2 * Math.Sqrt(
                            (A + 1 / A) * (1 / S - 1) + 2
                        );
                        double sqrtA2alpha = 2 * Math.Sqrt(A) * alpha;

                        double numReal = A * ((A + 1) + (A - 1) * cosW + sqrtA2alpha);
                        double numImag = -2 * A * Math.Sqrt(Math.Max(0, (A - 1) + (A + 1) * cosW));
                        double num = Math.Sqrt(numReal * numReal + numImag * numImag);

                        double denReal = (A + 1) - (A - 1) * cosW + sqrtA2alpha;
                        double denImag = 2 * Math.Sqrt(Math.Max(0, (A - 1) - (A + 1) * cosW));
                        double den = Math.Sqrt(denReal * denReal + denImag * denImag);

                        magnitudeDb = 20 * Math.Log10(num / den);
                    }
                    break;

                case 3: // Lowpass
                    {
                        double alpha = sinW / (2 * band.Q);
                        double numReal = (1 - cosW) / 2;
                        double denReal = 1 + alpha;
                        magnitudeDb = 20 * Math.Log10(numReal / denReal);
                    }
                    break;

                case 4: // Highpass
                    {
                        double alpha = sinW / (2 * band.Q);
                        double numReal = (1 + cosW) / 2;
                        double denReal = 1 + alpha;
                        magnitudeDb = 20 * Math.Log10(numReal / denReal);
                    }
                    break;

                case 5: // Notch
                    {
                        double alpha = sinW / (2 * band.Q);
                        double num = 1;
                        double den = 1 + alpha;
                        magnitudeDb = 20 * Math.Log10(num / den);
                    }
                    break;
            }

            return magnitudeDb;
        }

        /// <summary>
        /// Draw grid lines for frequency and dB markers.
        /// </summary>
        public void DrawGrid(
            Canvas? canvas,
            double canvasWidth,
            double canvasHeight)
        {
            if (canvas == null) return;
            if (canvasWidth <= 0 || canvasHeight <= 0) return;

            canvas.Children.Clear();

            // Draw 0dB line (center)
            var centerLine = new Line
            {
                X1 = 0,
                Y1 = canvasHeight / 2,
                X2 = canvasWidth,
                Y2 = canvasHeight / 2,
                Stroke = Microsoft.UI.ColorHelper.FromArgb(255, 255, 255, 255),
                StrokeThickness = 1
            };
            canvas.Children.Add(centerLine);

            // Draw +6dB and -6dB lines
            var posLine = new Line
            {
                X1 = 0,
                Y1 = canvasHeight / 4,
                X2 = canvasWidth,
                Y2 = canvasHeight / 4,
                Stroke = Microsoft.UI.ColorHelper.FromArgb(128, 255, 255, 255),
                StrokeThickness = 1,
                StrokeDashArray = new DoubleCollection { 4, 4 }
            };
            canvas.Children.Add(posLine);

            var negLine = new Line
            {
                X1 = 0,
                Y1 = 3 * canvasHeight / 4,
                X2 = canvasWidth,
                Y2 = 3 * canvasHeight / 4,
                Stroke = Microsoft.UI.ColorHelper.FromArgb(128, 255, 255, 255),
                StrokeThickness = 1,
                StrokeDashArray = new DoubleCollection { 4, 4 }
            };
            canvas.Children.Add(negLine);
        }
    }
}
