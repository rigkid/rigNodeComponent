#include "MidiBus.h"

namespace rig {
namespace node {
namespace {

MidiInValues g_in;
std::vector<MidiOutEvent> g_out;

} // namespace

void setMidiIn(const MidiInValues& v) {
	g_in = v;
}

MidiInValues getMidiIn() {
	return g_in;
}

void pushMidiOut(uint8_t status, uint8_t data1, uint8_t data2) {
	if (g_out.size() < 1024) {
		g_out.push_back({status, data1, data2});
	}
}

void takeMidiOut(std::vector<MidiOutEvent>& dest) {
	dest.swap(g_out);
	g_out.clear();
}

} // namespace node
} // namespace rig
