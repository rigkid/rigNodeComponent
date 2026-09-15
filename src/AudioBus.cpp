#include "AudioBus.h"

namespace rig {
namespace node {
namespace {

AudioBusValues g_bus;
AudioSpectrum g_spectrum;
CvBusValues g_cv;
TempoBusValues g_tempo;

} // namespace

void setAudioBus(const AudioBusValues& v) {
	g_bus = v;
}

void setAudioSpectrum(const AudioSpectrum& s) {
	g_spectrum = s;
}

void setCvBus(const CvBusValues& v) {
	g_cv = v;
}

void setTempoBus(const TempoBusValues& v) {
	g_tempo = v;
}

AudioBusValues getAudioBus() {
	return g_bus;
}

const AudioSpectrum& getAudioSpectrum() {
	return g_spectrum;
}

CvBusValues getCvBus() {
	return g_cv;
}

TempoBusValues getTempoBus() {
	return g_tempo;
}

} // namespace node
} // namespace rig
