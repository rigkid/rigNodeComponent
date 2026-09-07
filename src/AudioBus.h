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

/** @brief Publish this frame's audio analysis (main thread, once per frame). */
void setAudioBus(const AudioBusValues& v);

/** @brief Last published analysis (all zeros when no publisher runs). */
AudioBusValues getAudioBus();

} // namespace node
} // namespace rig
