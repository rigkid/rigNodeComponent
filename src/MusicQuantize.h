#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rig {
namespace node {

/** @brief Scale degrees from the tonic (0 = root). Chromatic is a no-op. */
enum class MusicScale {
	Chromatic = 0,
	Major = 1,
	Minor = 2,
	HarmonicMinor = 3,
	Dorian = 4,
	Mixolydian = 5,
	PentaMajor = 6,
	PentaMinor = 7,
	Blues = 8,
	WholeTone = 9,
	Count = 10
};

/** @brief Pitch-class degrees for @p scale (empty / null = chromatic). */
inline const uint8_t* musicScaleDegrees(MusicScale scale, int& count) {
	static constexpr uint8_t kMaj[] = {0, 2, 4, 5, 7, 9, 11};
	static constexpr uint8_t kMin[] = {0, 2, 3, 5, 7, 8, 10};
	static constexpr uint8_t kHarm[] = {0, 2, 3, 5, 7, 8, 11};
	static constexpr uint8_t kDor[] = {0, 2, 3, 5, 7, 9, 10};
	static constexpr uint8_t kMix[] = {0, 2, 4, 5, 7, 9, 10};
	static constexpr uint8_t kPmaj[] = {0, 2, 4, 7, 9};
	static constexpr uint8_t kPmin[] = {0, 3, 5, 7, 10};
	static constexpr uint8_t kBlu[] = {0, 3, 5, 6, 7, 10};
	static constexpr uint8_t kWt[] = {0, 2, 4, 6, 8, 10};
	switch (scale) {
	case MusicScale::Major:
		count = 7;
		return kMaj;
	case MusicScale::Minor:
		count = 7;
		return kMin;
	case MusicScale::HarmonicMinor:
		count = 7;
		return kHarm;
	case MusicScale::Dorian:
		count = 7;
		return kDor;
	case MusicScale::Mixolydian:
		count = 7;
		return kMix;
	case MusicScale::PentaMajor:
		count = 5;
		return kPmaj;
	case MusicScale::PentaMinor:
		count = 5;
		return kPmin;
	case MusicScale::Blues:
		count = 6;
		return kBlu;
	case MusicScale::WholeTone:
		count = 6;
		return kWt;
	default:
		count = 0;
		return nullptr;
	}
}

/**
 * @brief Snap a MIDI note to the nearest pitch in @p scale (root 0=C .. 11=B).
 * @details Ties prefer the lower note so a held value does not chatter.
 */
inline int quantizeMidi(float midi, int root, MusicScale scale) {
	const int rounded = static_cast<int>(std::lround(midi));
	const int clamped = std::clamp(rounded, 0, 127);
	int n = 0;
	const uint8_t* deg = musicScaleDegrees(scale, n);
	if (!deg || n <= 0) {
		return clamped;
	}
	root = ((root % 12) + 12) % 12;
	int best = clamped;
	int bestDist = 128;
	for (int o = clamped - 12; o <= clamped + 12; ++o) {
		if (o < 0 || o > 127) {
			continue;
		}
		const int pc = ((o % 12) + 12) % 12;
		const int rel = (pc - root + 12) % 12;
		bool inScale = false;
		for (int i = 0; i < n; ++i) {
			if (deg[i] == static_cast<uint8_t>(rel)) {
				inScale = true;
				break;
			}
		}
		if (!inScale) {
			continue;
		}
		const int d = std::abs(o - clamped);
		if (d < bestDist || (d == bestDist && o < best)) {
			bestDist = d;
			best = o;
		}
	}
	return best;
}

/** @brief 0..1 control value treated as MIDI 0..127, then quantized. */
inline float quantizeUnit(float unit01, int root, MusicScale scale) {
	const float midi = std::clamp(unit01, 0.f, 1.f) * 127.f;
	return static_cast<float>(quantizeMidi(midi, root, scale)) / 127.f;
}

/** @brief Scale degree 0..1 for a quantized MIDI note (0 = tonic). */
inline float scaleDegree01(int midi, int root, MusicScale scale) {
	int n = 0;
	const uint8_t* deg = musicScaleDegrees(scale, n);
	if (!deg || n <= 0) {
		return static_cast<float>(((midi % 12) + 12) % 12) / 12.f;
	}
	root = ((root % 12) + 12) % 12;
	const int pc = ((midi % 12) + 12) % 12;
	const int rel = (pc - root + 12) % 12;
	for (int i = 0; i < n; ++i) {
		if (deg[i] == static_cast<uint8_t>(rel)) {
			return static_cast<float>(i) / static_cast<float>(n);
		}
	}
	return 0.f;
}

} // namespace node
} // namespace rig
