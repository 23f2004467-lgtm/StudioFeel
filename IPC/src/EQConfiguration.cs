// ============================================================================
// StudioFeel — EQ Configuration (C# Shared Types)
// ============================================================================
// C# version of the EQ data structures for use by the UI.
// These mirror the C++ structures in include/EQParameters.h
// ============================================================================

using System.Text.Json.Serialization;

namespace StudioFeel.IPC
{
    /// <summary>
    /// Filter types supported by the EQ.
    /// </summary>
    public enum FilterType
    {
        Peaking = 0,
        LowShelf = 1,
        HighShelf = 2,
        Lowpass = 3,
        Highpass = 4,
        Notch = 5
    }

    /// <summary>
    /// Configuration for a single EQ band.
    /// </summary>
    public class EQBandConfig
    {
        [JsonPropertyName("type")]
        [JsonConverter(typeof(JsonStringEnumConverter))]
        public FilterType Type { get; set; } = FilterType.Peaking;

        [JsonPropertyName("frequencyHz")]
        public float Frequency { get; set; } = 1000.0f;

        [JsonPropertyName("Q")]
        public float Q { get; set; } = 1.0f;

        [JsonPropertyName("gainDb")]
        public float GainDb { get; set; } = 0.0f;

        [JsonPropertyName("enabled")]
        public bool Enabled { get; set; } = true;

        [JsonPropertyName("label")]
        public string? Label { get; set; }

        /// <summary>
        /// Clamp all values to valid ranges.
        /// </summary>
        public void Clamp()
        {
            Frequency = Math.Clamp(Frequency, 20.0f, 20000.0f);
            Q = Math.Clamp(Q, 0.1f, 10.0f);
            GainDb = Math.Clamp(GainDb, -24.0f, 24.0f);
        }
    }

    /// <summary>
    /// Complete EQ configuration.
    /// </summary>
    public class EQConfiguration
    {
        public const int MaxBands = 10;

        [JsonPropertyName("version")]
        public string Version { get; set; } = "1.0";

        [JsonPropertyName("masterEnabled")]
        public bool MasterEnabled { get; set; } = true;

        [JsonPropertyName("masterGainDb")]
        public float MasterGainDb { get; set; } = 0.0f;

        [JsonPropertyName("sampleRate")]
        public uint SampleRate { get; set; } = 48000;

        [JsonPropertyName("bands")]
        public List<EQBandConfig> Bands { get; set; } = new();

        /// <summary>
        /// Clamp all values to valid ranges.
        /// </summary>
        public void Clamp()
        {
            MasterGainDb = Math.Clamp(MasterGainDb, -12.0f, 12.0f);
            if (Bands.Count > MaxBands)
            {
                Bands = Bands.Take(MaxBands).ToList();
            }
            foreach (var band in Bands)
            {
                band.Clamp();
            }
        }
    }
}
