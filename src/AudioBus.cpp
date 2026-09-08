#include "AudioBus.h"

namespace rig {
namespace node {
namespace {

AudioBusValues g_bus;
AudioSpectrum g_spectrum;

} // namespace

void setAudioBus(const AudioBusValues& v) {
	g_bus = v;
}

void setAudioSpectrum(const AudioSpectrum& s) {
	g_spectrum = s;
}

AudioBusValues getAudioBus() {
	return g_bus;
}

const AudioSpectrum& getAudioSpectrum() {
	return g_spectrum;
}

} // namespace node
} // namespace rig
