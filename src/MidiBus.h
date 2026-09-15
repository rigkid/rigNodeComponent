#pragma once

#include <cstdint>
#include <vector>

namespace rig {
namespace node {

/**
 * @brief Live MIDI input snapshot read by `midi.in` nodes.
 * @details A MIDI pack (rigMidi) publishes this once per frame from its
 *          device queue. `gate` is monophonic (last note-on until matching
 *          note-off). `noteOn` / `noteOff` are single-frame pulses.
 */
struct MidiInValues {
	static constexpr int kCc = 128;
	bool active = false;
	bool gate = false;
	bool noteOn = false;
	bool noteOff = false;
	float note = 60.f;
	float vel = 0.f;
	int channel = 1; ///< 1-16 of the last voice message.
	int lastCc = -1;
	float lastCcVal = 0.f; ///< 0..1
	float cc[kCc]{};	   ///< Last value per CC, 0..1.
};

struct MidiOutEvent {
	uint8_t status = 0;
	uint8_t data1 = 0;
	uint8_t data2 = 0;
};

void setMidiIn(const MidiInValues& v);
MidiInValues getMidiIn();

/** @brief Queue a short message for the MIDI pack to send this frame. */
void pushMidiOut(uint8_t status, uint8_t data1, uint8_t data2);
/** @brief Drain the out queue (swap). Call from the MIDI pack after graph eval. */
void takeMidiOut(std::vector<MidiOutEvent>& dest);

} // namespace node
} // namespace rig
