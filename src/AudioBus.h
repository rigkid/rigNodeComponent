#pragma once

namespace rig {
namespace node {

/**
 * @brief Live audio analysis values read by `mod.audio` nodes.
 * @details An audio capture pack (e.g. rigAudio) publishes these once per frame on the
 * main thread; graph eval reads them the same frame. Values are normalized to roughly
 * 0..1 by the publisher. `active` is false while no capture device is running so audio
 * nodes idle at zero instead of holding a stale level.
 */
struct AudioBusValues {
	float level = 0.f;	///< Broadband loudness (RMS, auto-levelled)
	float peak = 0.f;	///< Fast peak follower
	float bass = 0.f;	///< Energy below ~200 Hz
	float mid = 0.f;	///< Energy ~200 Hz .. 2 kHz
	float treble = 0.f; ///< Energy above ~2 kHz
	bool active = false;
};

/**
 * @brief Waveform + FFT packed the way ISF hosts serve `audio` / `audioFFT`.
 * @details Two rows (L/R). Wave samples are 0.5-centered amplitude. FFT bins
 * are 0..1 magnitudes from DC (x=0) to Nyquist (last column).
 */
struct AudioSpectrum {
	static constexpr int kChannels = 2;
	static constexpr int kWave = 1024;
	static constexpr int kFft = 512;
	float wave[kChannels][kWave]{};
	float fft[kChannels][kFft]{};
	int channels = 2;
	bool active = false;

	AudioSpectrum() {
		for (int ch = 0; ch < kChannels; ++ch) {
			for (int i = 0; i < kWave; ++i) {
				wave[ch][i] = 0.5f;
			}
		}
	}
};

/**
 * @brief Live control voltages read by `mod.cv` nodes.
 * @details A capture pack (e.g. rigAudio driving a DC-coupled interface such as
 * an Expert Sleepers ES-8/9) publishes per-channel DC levels once per frame.
 * Values are normalized -1..1 where full scale is the interface's CV range
 * (±10V on the ES-9). `active` is false while no CV device is running.
 */
struct CvBusValues {
	static constexpr int kMaxChannels = 16;
	float value[kMaxChannels]{};
	int channels = 0;
	bool active = false;
};

/** @brief Publish this frame's audio analysis (main thread, once per frame). */
void setAudioBus(const AudioBusValues& v);
void setAudioSpectrum(const AudioSpectrum& s);
void setCvBus(const CvBusValues& v);

/** @brief Last published analysis (all zeros when no publisher runs). */
AudioBusValues getAudioBus();
const AudioSpectrum& getAudioSpectrum();
CvBusValues getCvBus();

} // namespace node
} // namespace rig
