from datetime import datetime, timezone
import math
import unittest

import numpy as np

from singapore_sensor_fusion.synthetic import (
    BAND_2_4_GHZ,
    BAND_5_8_GHZ,
    PLACEHOLDER_PREPROCESSING,
    generate_synthetic_iq,
    welch_psd,
)


class SyntheticSignalTests(unittest.TestCase):
    def test_synthetic_iq_is_reproducible_and_immutable(self) -> None:
        timestamp = datetime(2026, 8, 1, tzinfo=timezone.utc)
        parameters = dict(
            center_frequency_hz=BAND_2_4_GHZ,
            sample_rate_hz=20e6,
            num_samples=4096,
            signal_power_dbm=-80.0,
            noise_power_dbm=-96.0,
            seed=19,
            timestamp=timestamp,
        )
        first = generate_synthetic_iq(**parameters)
        second = generate_synthetic_iq(**parameters)
        np.testing.assert_array_equal(first.samples, second.samples)
        self.assertEqual(first.samples.dtype, np.complex64)
        self.assertFalse(first.samples.flags.writeable)
        self.assertEqual(first.preprocessing, PLACEHOLDER_PREPROCESSING)

    def test_256_bin_psd_tracks_a_tone_at_both_centres(self) -> None:
        sample_rate_hz = 20e6
        offset_hz = 1.25e6  # exactly sixteen bins at 20 MHz / 256
        for center_frequency_hz in (BAND_2_4_GHZ, BAND_5_8_GHZ):
            with self.subTest(center_frequency_hz=center_frequency_hz):
                frame = generate_synthetic_iq(
                    center_frequency_hz=center_frequency_hz,
                    sample_rate_hz=sample_rate_hz,
                    num_samples=4096,
                    signal_power_dbm=-40.0,
                    noise_power_dbm=-120.0,
                    signal_offset_hz=offset_hz,
                    waveform="tone",
                    seed=3,
                )
                psd = welch_psd(frame)
                self.assertEqual(psd.frequency_hz.shape, (256,))
                self.assertEqual(psd.power_dbm_per_hz.shape, (256,))
                peak_frequency = psd.frequency_hz[int(np.argmax(psd.power_dbm_per_hz))]
                self.assertAlmostEqual(
                    peak_frequency,
                    center_frequency_hz + offset_hz,
                    delta=1.0,
                )

                bin_width_hz = sample_rate_hz / 256.0
                integrated_power_mw = float(
                    np.sum(10.0 ** (psd.power_dbm_per_hz / 10.0)) * bin_width_hz
                )
                integrated_power_dbm = 10.0 * math.log10(integrated_power_mw)
                self.assertAlmostEqual(integrated_power_dbm, -40.0, delta=0.15)


if __name__ == "__main__":
    unittest.main()
