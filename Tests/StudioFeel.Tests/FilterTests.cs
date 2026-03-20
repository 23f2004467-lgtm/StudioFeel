// ============================================================================
// StudioFeel — Unit Tests
// ============================================================================

using Xunit;
using StudioFeel.IPC;

namespace StudioFeel.Tests
{
    /// <summary>
    /// Tests for filter calculations and EQ band operations.
    /// </summary>
    public class FilterTests
    {
        [Fact]
        public void EQBandConfig_Clamp_ValidatesRanges()
        {
            var band = new EQBandConfig
            {
                frequency = 50000,  // Above max
                Q = 50,            // Above max
                gainDb = 100       // Above max
            };

            band.Clamp();

            Assert.Equal(20000, band.frequency);
            Assert.Equal(10, band.Q);
            Assert.Equal(24, band.gainDb);
        }

        [Fact]
        public void EQBandConfig_Clamp_HandlesMinimum()
        {
            var band = new EQBandConfig
            {
                frequency = 5,   // Below min
                Q = 0.01,        // Below min
                gainDb = -100    // Below min
            };

            band.Clamp();

            Assert.Equal(20, band.frequency);
            Assert.Equal(0.1, band.Q);
            Assert.Equal(-24, band.gainDb);
        }

        [Fact]
        public void EQBandConfig_Equality_Works()
        {
            var band1 = new EQBandConfig
            {
                type = FilterType.Peaking,
                frequency = 1000,
                Q = 1.0,
                gainDb = 3.0,
                enabled = true
            };

            var band2 = new EQBandConfig
            {
                type = FilterType.Peaking,
                frequency = 1000,
                Q = 1.0,
                gainDb = 3.0,
                enabled = true
            };

            Assert.Equal(band1, band2);
            Assert.True(band1 == band2);
        }

        [Fact]
        public void EQConfiguration_Clamp_ValidatesAllBands()
        {
            var config = new EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 100,  // Above max
                sampleRate = 48000,
                bands = new List<EQBandConfig>
                {
                    new EQBandConfig { frequency = 50000, Q = 50, gainDb = 100 },
                    new EQBandConfig { frequency = 5, Q = 0.01, gainDb = -100 }
                }
            };

            config.Clamp();

            Assert.Equal(12, config.masterGainDb);
            Assert.Equal(20000, config.bands[0].frequency);
            Assert.Equal(20, config.bands[1].frequency);
        }

        [Fact]
        public void FilterTypeToString_ReturnsCorrectStrings()
        {
            Assert.Equal("peaking", FilterTypeToString(FilterType.Peaking));
            Assert.Equal("lowshelf", FilterTypeToString(FilterType.LowShelf));
            Assert.Equal("highshelf", FilterTypeToString(FilterType.HighShelf));
            Assert.Equal("lowpass", FilterTypeToString(FilterType.Lowpass));
            Assert.Equal("highpass", FilterTypeToString(FilterType.Highpass));
            Assert.Equal("notch", FilterTypeToString(FilterType.Notch));
        }

        [Fact]
        public void FilterTypeFromString_ParsesCorrectly()
        {
            Assert.Equal(FilterType.Peaking, FilterTypeFromString("peaking"));
            Assert.Equal(FilterType.LowShelf, FilterTypeFromString("lowshelf"));
            Assert.Equal(FilterType.HighShelf, FilterTypeFromString("highshelf"));
            Assert.Equal(FilterType.Lowpass, FilterTypeFromString("lowpass"));
            Assert.Equal(FilterType.Highpass, FilterTypeFromString("highpass"));
            Assert.Equal(FilterType.Notch, FilterTypeFromString("notch"));
        }
    }

    /// <summary>
    /// Tests for JSON serialization/deserialization.
    /// </summary>
    public class JsonTests
    {
        [Fact]
        public void SerializeFlatConfig_ProducesValidJson()
        {
            var config = new EQConfiguration
            {
                masterEnabled = true,
                masterGainDb = 0,
                sampleRate = 48000,
                bands = new List<EQBandConfig>
                {
                    new EQBandConfig
                    {
                        type = FilterType.Peaking,
                        frequency = 1000,
                        Q = 1.0,
                        gainDb = 0,
                        enabled = true
                    }
                }
            };

            string json = Json.SerializeConfiguration(config);

            Assert.Contains("\"version\"", json);
            Assert.Contains("\"masterEnabled\": true", json);
            Assert.Contains("\"bands\"", json);
        }

        [Fact]
        public void DeserializeConfig_ParsesCorrectly()
        {
            string json = @"{
                ""version"": ""1.0"",
                ""masterEnabled"": true,
                ""masterGainDb"": 3.5,
                ""sampleRate"": 48000,
                ""bands"": [
                    {
                        ""index"": 0,
                        ""type"": ""peaking"",
                        ""frequencyHz"": 1000,
                        ""Q"": 1.5,
                        ""gainDb"": 6.0,
                        ""enabled"": true
                    }
                ]
            }";

            bool success = Json.DeserializeConfiguration(json, out var config);

            Assert.True(success);
            Assert.True(config.masterEnabled);
            Assert.Equal(3.5, config.masterGainDb);
            Assert.Single(config.bands);
            Assert.Equal(FilterType.Peaking, config.bands[0].type);
            Assert.Equal(1000, config.bands[0].frequency);
            Assert.Equal(1.5, config.bands[0].Q);
            Assert.Equal(6.0, config.bands[0].gainDb);
        }
    }
}
