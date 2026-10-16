#pragma once

#include "PictureFeatures.h"

namespace rig {
namespace node {

/**
 * @brief Live audio analysis values read by `mod.audio` / `mod.note` nodes.
 * @details An audio capture pack (e.g. rigAudio) publishes these once per frame on the
 * main thread; graph eval reads them the same frame. Level / bands / peak are
 * normalized to roughly 0..1 by the publisher. Spectral fields are 0..1 control
 * values (centroid / hue / warmth / flux / onset) plus pitch as Hz / MIDI.
 * `active` is false while no capture device is running so audio nodes idle at
 * zero instead of holding a stale level.
 */
struct AudioBusValues {
	float level = 0.f;	  ///< Broadband loudness (RMS, auto-levelled)
	float peak = 0.f;	  ///< Fast peak follower
	float bass = 0.f;	  ///< Energy below ~200 Hz
	float mid = 0.f;	  ///< Energy ~200 Hz .. 2 kHz
	float treble = 0.f;	  ///< Energy above ~2 kHz
	float centroid = 0.f; ///< 0..1 spectral brightness (timbre / colour)
	float spread = 0.f;	  ///< 0..1 bandwidth around the centroid
	float rolloff = 0.f;  ///< 0..1 frequency holding 85% of energy
	float flatness = 0.f; ///< 0 = tone, 1 = noise
	float flux = 0.f;	  ///< 0..1 spectral change (onset energy)
	float hue = 0.f;	  ///< 0..1 colour from centroid (warm to cool)
	float warmth = 0.f;	  ///< 0..1 bass vs treble
	float colorR = 0.f;	  ///< Shared feedback rgba from hue + energy
	float colorG = 0.f;
	float colorB = 0.f;
	float colorA = 1.f;
	float temperature = 0.5f; ///< Shared name: same as warmth (0 cold, 1 warm)
	float onset = 0.f;		  ///< 1 on a detected attack this frame
	float pitchHz = 0.f;	  ///< Detected fundamental
	float note = 0.f;		  ///< Fractional MIDI note
	float chroma = 0.f;		  ///< Pitch class 0..1 (C = 0)
	float pitchConf = 0.f;
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

/**
 * @brief Shared musical clock read by `mod.beat` nodes.
 * @details The host publishes once per frame from whatever source it has (tap
 * tempo, MIDI clock, Link, beat detection) - the bus doesn't care. `beat` is
 * the running quarter-note count; a double keeps phase precise over long sets.
 * While `active` is false, `mod.beat` free-runs from eval time at its own
 * BPM param, so patches still animate without a clock source.
 */
struct TempoBusValues {
	double beat = 0.0; ///< Running beat count (quarter notes)
	float bpm = 120.f;
	float beatsPerBar = 4.f;
	bool active = false;
	bool detected = false; ///< True when an audio beat tracker is contributing
	float confidence = 0.f;
};

/** @brief Live picture analysis (dominant color + temperature). */
struct PictureBusValues {
	float colorR = 0.f;
	float colorG = 0.f;
	float colorB = 0.f;
	float colorA = 1.f;
	float temperature = 0.5f; ///< 0 = cold, 1 = warm
	bool active = false;
};

/** @brief Publish this frame's audio analysis (main thread, once per frame). */
void setAudioBus(const AudioBusValues& v);
void setAudioSpectrum(const AudioSpectrum& s);
void setCvBus(const CvBusValues& v);
void setTempoBus(const TempoBusValues& v);
void setPictureBus(const PictureBusValues& v);

/** @brief Publish a picture hop (main thread). */
inline void publishPictureFrame(const rigkit::picture::PictureFeatureFrame& f) {
	PictureBusValues v;
	v.colorR = f.colorR;
	v.colorG = f.colorG;
	v.colorB = f.colorB;
	v.colorA = f.colorA;
	v.temperature = f.temperature;
	v.active = true;
	setPictureBus(v);
}

/** @brief Last published analysis (all zeros when no publisher runs). */
AudioBusValues getAudioBus();
const AudioSpectrum& getAudioSpectrum();
CvBusValues getCvBus();
TempoBusValues getTempoBus();
PictureBusValues getPictureBus();

} // namespace node
} // namespace rig
