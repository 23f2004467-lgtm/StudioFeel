#pragma once
// ============================================================================
// StudioFeel — Biquad Coefficient Calculator
// ============================================================================
// Implements the Robert Bristow-Johnson Audio EQ Cookbook formulas for
// computing second-order IIR (biquad) filter coefficients.
//
// Reference: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
//
// All computation is done in double precision for accuracy, then stored
// as float coefficients. Coefficient recalculation should happen on a
// non-real-time thread; only the resulting BiquadCoefficients struct
// is passed to the audio thread.
// ============================================================================

#include "BiquadFilter.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

namespace StudioFeel {

class CoefficientCalculator {
public:
    // ========================================================================
    // Peaking (Parametric) EQ
    // ========================================================================
    // Boost or cut centered at 'frequency' with bandwidth controlled by Q.
    // gainDb: positive = boost, negative = cut.
    //
    // H(s) = (s^2 + s*(A/Q) + 1) / (s^2 + s/(A*Q) + 1)
    // ========================================================================
    static BiquadCoefficients PeakingEQ(float frequency, float sampleRate,
                                         float Q, float gainDb)
    {
        if (std::abs(gainDb) < 0.01f) return BiquadCoefficients::Identity();

        const double A     = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double omega = 2.0 * M_PI * static_cast<double>(frequency)
                           / static_cast<double>(sampleRate);
        const double sinW  = std::sin(omega);
        const double cosW  = std::cos(omega);
        const double alpha = sinW / (2.0 * static_cast<double>(Q));

        BiquadCoefficients c;
        const float b0 = static_cast<float>(1.0 + alpha * A);
        const float b1 = static_cast<float>(-2.0 * cosW);
        const float b2 = static_cast<float>(1.0 - alpha * A);
        const float a0 = static_cast<float>(1.0 + alpha / A);
        const float a1 = static_cast<float>(-2.0 * cosW);
        const float a2 = static_cast<float>(1.0 - alpha / A);

        c.b0 = b0; c.b1 = b1; c.b2 = b2;
        c.a1 = a1; c.a2 = a2;
        c.Normalize(a0);
        return c;
    }

    // ========================================================================
    // Low Shelf
    // ========================================================================
    // Boost or cut frequencies below 'frequency'.
    // S: shelf slope (1.0 = steepest for a given gain). Typical: 0.5 - 1.0.
    //    When S = 1, the shelf slope is as steep as possible while remaining
    //    monotonically increasing or decreasing.
    //
    // H(s) = A * [ (A+1) - (A-1)*cos(w) + 2*sqrt(A)*alpha ]
    //         / [ (A+1) + (A-1)*cos(w) + 2*sqrt(A)*alpha ]
    // ========================================================================
    static BiquadCoefficients LowShelf(float frequency, float sampleRate,
                                        float S, float gainDb)
    {
        if (std::abs(gainDb) < 0.01f) return BiquadCoefficients::Identity();

        const double A     = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double omega = 2.0 * M_PI * static_cast<double>(frequency)
                           / static_cast<double>(sampleRate);
        const double sinW  = std::sin(omega);
        const double cosW  = std::cos(omega);
        const double alpha = sinW / 2.0
                           * std::sqrt((A + 1.0 / A) * (1.0 / static_cast<double>(S) - 1.0) + 2.0);
        const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;

        BiquadCoefficients c;
        const float a0 = static_cast<float>( (A + 1.0) + (A - 1.0) * cosW + sqrtA2alpha);
        c.b0 = static_cast<float>(A * ((A + 1.0) - (A - 1.0) * cosW + sqrtA2alpha));
        c.b1 = static_cast<float>(2.0 * A * ((A - 1.0) - (A + 1.0) * cosW));
        c.b2 = static_cast<float>(A * ((A + 1.0) - (A - 1.0) * cosW - sqrtA2alpha));
        c.a1 = static_cast<float>(-2.0 * ((A - 1.0) + (A + 1.0) * cosW));
        c.a2 = static_cast<float>( (A + 1.0) + (A - 1.0) * cosW - sqrtA2alpha);
        c.Normalize(a0);
        return c;
    }

    // ========================================================================
    // High Shelf
    // ========================================================================
    // Boost or cut frequencies above 'frequency'.
    // S: shelf slope (same semantics as LowShelf).
    // ========================================================================
    static BiquadCoefficients HighShelf(float frequency, float sampleRate,
                                         float S, float gainDb)
    {
        if (std::abs(gainDb) < 0.01f) return BiquadCoefficients::Identity();

        const double A     = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const double omega = 2.0 * M_PI * static_cast<double>(frequency)
                           / static_cast<double>(sampleRate);
        const double sinW  = std::sin(omega);
        const double cosW  = std::cos(omega);
        const double alpha = sinW / 2.0
                           * std::sqrt((A + 1.0 / A) * (1.0 / static_cast<double>(S) - 1.0) + 2.0);
        const double sqrtA2alpha = 2.0 * std::sqrt(A) * alpha;

        BiquadCoefficients c;
        const float a0 = static_cast<float>( (A + 1.0) - (A - 1.0) * cosW + sqrtA2alpha);
        c.b0 = static_cast<float>(A * ((A + 1.0) + (A - 1.0) * cosW + sqrtA2alpha));
        c.b1 = static_cast<float>(-2.0 * A * ((A - 1.0) + (A + 1.0) * cosW));
        c.b2 = static_cast<float>(A * ((A + 1.0) + (A - 1.0) * cosW - sqrtA2alpha));
        c.a1 = static_cast<float>( 2.0 * ((A - 1.0) - (A + 1.0) * cosW));
        c.a2 = static_cast<float>( (A + 1.0) - (A - 1.0) * cosW - sqrtA2alpha);
        c.Normalize(a0);
        return c;
    }

    // ========================================================================
    // Low-Pass Filter
    // ========================================================================
    // Passes frequencies below 'frequency', attenuates above.
    // Q controls the resonance at the cutoff. Q = 0.707 = Butterworth (flat).
    //
    // H(s) = 1 / (s^2 + s/Q + 1)
    // ========================================================================
    static BiquadCoefficients Lowpass(float frequency, float sampleRate, float Q)
    {
        const double omega = 2.0 * M_PI * static_cast<double>(frequency)
                           / static_cast<double>(sampleRate);
        const double sinW  = std::sin(omega);
        const double cosW  = std::cos(omega);
        const double alpha = sinW / (2.0 * static_cast<double>(Q));

        BiquadCoefficients c;
        const float a0 = static_cast<float>(1.0 + alpha);
        c.b0 = static_cast<float>((1.0 - cosW) / 2.0);
        c.b1 = static_cast<float>( 1.0 - cosW);
        c.b2 = static_cast<float>((1.0 - cosW) / 2.0);
        c.a1 = static_cast<float>(-2.0 * cosW);
        c.a2 = static_cast<float>( 1.0 - alpha);
        c.Normalize(a0);
        return c;
    }

    // ========================================================================
    // High-Pass Filter
    // ========================================================================
    // Passes frequencies above 'frequency', attenuates below.
    // Q = 0.707 for Butterworth (flat passband).
    //
    // H(s) = s^2 / (s^2 + s/Q + 1)
    // ========================================================================
    static BiquadCoefficients Highpass(float frequency, float sampleRate, float Q)
    {
        const double omega = 2.0 * M_PI * static_cast<double>(frequency)
                           / static_cast<double>(sampleRate);
        const double sinW  = std::sin(omega);
        const double cosW  = std::cos(omega);
        const double alpha = sinW / (2.0 * static_cast<double>(Q));

        BiquadCoefficients c;
        const float a0 = static_cast<float>(1.0 + alpha);
        c.b0 = static_cast<float>((1.0 + cosW) / 2.0);
        c.b1 = static_cast<float>(-(1.0 + cosW));
        c.b2 = static_cast<float>((1.0 + cosW) / 2.0);
        c.a1 = static_cast<float>(-2.0 * cosW);
        c.a2 = static_cast<float>( 1.0 - alpha);
        c.Normalize(a0);
        return c;
    }

    // ========================================================================
    // Notch (Band-Reject) Filter
    // ========================================================================
    // Rejects frequencies near 'frequency'. Q controls the width of rejection.
    //
    // H(s) = (s^2 + 1) / (s^2 + s/Q + 1)
    // ========================================================================
    static BiquadCoefficients Notch(float frequency, float sampleRate, float Q)
    {
        const double omega = 2.0 * M_PI * static_cast<double>(frequency)
                           / static_cast<double>(sampleRate);
        const double sinW  = std::sin(omega);
        const double cosW  = std::cos(omega);
        const double alpha = sinW / (2.0 * static_cast<double>(Q));

        BiquadCoefficients c;
        const float a0 = static_cast<float>(1.0 + alpha);
        c.b0 = static_cast<float>(1.0);
        c.b1 = static_cast<float>(-2.0 * cosW);
        c.b2 = static_cast<float>(1.0);
        c.a1 = static_cast<float>(-2.0 * cosW);
        c.a2 = static_cast<float>( 1.0 - alpha);
        c.Normalize(a0);
        return c;
    }

    // ========================================================================
    // Convenience: compute coefficients from an EQBandConfig
    // ========================================================================
    static BiquadCoefficients FromBandConfig(const struct EQBandConfig& band,
                                              float sampleRate)
    {
        // Forward declaration resolved at link time
        switch (band.type) {
            case FilterType::Peaking:
                return PeakingEQ(band.frequency, sampleRate, band.Q, band.gainDb);
            case FilterType::LowShelf:
                return LowShelf(band.frequency, sampleRate, 1.0f, band.gainDb);
            case FilterType::HighShelf:
                return HighShelf(band.frequency, sampleRate, 1.0f, band.gainDb);
            case FilterType::Lowpass:
                return Lowpass(band.frequency, sampleRate, band.Q);
            case FilterType::Highpass:
                return Highpass(band.frequency, sampleRate, band.Q);
            case FilterType::Notch:
                return Notch(band.frequency, sampleRate, band.Q);
            default:
                return BiquadCoefficients::Identity();
        }
    }
};

} // namespace StudioFeel
