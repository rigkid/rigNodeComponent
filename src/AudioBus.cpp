#include "AudioBus.h"

namespace rig {
namespace node {
namespace {

AudioBusValues g_bus;

} // namespace

void setAudioBus(const AudioBusValues& v) {
	g_bus = v;
}

AudioBusValues getAudioBus() {
	return g_bus;
}

} // namespace node
} // namespace rig
