#include "GraphEval.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include "AudioBus.h"
#include "MidiBus.h"
#include "NodeGraph.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rig {
namespace node {
namespace {

constexpr float kPi = static_cast<float>(M_PI);
constexpr float kTau = kPi * 2.f;

const rigkit::ecs::NodePin* pinByName(const rigkit::ecs::GraphNode& n, std::string_view name,
									  rigkit::ecs::NodePinKind kind) {
	for (const auto& p : n.pins) {
		if (p.kind == kind && p.name == name) {
			return &p;
		}
	}
	return nullptr;
}

glm::vec4 coerce(const glm::vec4& v, std::string_view fromTypeIn, std::string_view toTypeIn) {
	const std::string fromType = normalizePinType(fromTypeIn);
	const std::string toType = normalizePinType(toTypeIn);
	if (toType.empty() || fromType == toType) {
		return v;
	}
	if (toType == "float") {
		if (fromType == "vec4" || fromType == "vec3") {
			return glm::vec4((v.r + v.g + v.b) / 3.f);
		}
		return glm::vec4(v.x);
	}
	if (toType == "vec2") {
		if (fromType == "float") {
			return glm::vec4(v.x, v.x, 0.f, 0.f);
		}
		return glm::vec4(v.x, v.y, 0.f, 0.f);
	}
	if (toType == "vec4" || toType == "vec3" || toType == "quat") {
		if (fromType == "float") {
			return glm::vec4(v.x, v.x, v.x, 1.f);
		}
		if (fromType == "vec2") {
			return glm::vec4(v.x, v.y, 0.f, 1.f);
		}
	}
	return v;
}

glm::vec4 linkedIn(const rigkit::ecs::NodeGraphData& graph, const EvalResult& ev, uint32_t nodeId,
				   uint32_t pinId, std::string_view toType, const glm::vec4& fallback) {
	for (const auto& l : graph.links) {
		if (l.toNode != nodeId || l.toPin != pinId) {
			continue;
		}
		const auto* fromPin = graph.findPin(l.fromNode, l.fromPin);
		const auto it = ev.pinValue.find(pinKey(l.fromNode, l.fromPin));
		if (it == ev.pinValue.end()) {
			return fallback;
		}
		const std::string_view fromType = fromPin ? fromPin->type : std::string_view{};
		return coerce(it->second, fromType, toType);
	}
	// Injected values (group published inputs) keyed on the destination pin.
	const auto inj = ev.pinValue.find(pinKey(nodeId, pinId));
	if (inj != ev.pinValue.end()) {
		return coerce(inj->second, {}, toType);
	}
	return fallback;
}

bool pinHasIncoming(const rigkit::ecs::NodeGraphData& graph, uint32_t nodeId, uint32_t pinId) {
	for (const auto& l : graph.links) {
		if (l.toNode == nodeId && l.toPin == pinId) {
			return true;
		}
	}
	return false;
}

glm::vec4 inPin(const rigkit::ecs::NodeGraphData& graph, const EvalResult& ev,
				const rigkit::ecs::GraphNode& node, std::string_view name, std::string_view toType,
				const glm::vec4& fallback) {
	const auto* p = pinByName(node, name, rigkit::ecs::NodePinKind::In);
	if (!p) {
		return fallback;
	}
	const std::string_view want = toType.empty() ? std::string_view{p->type} : toType;
	return linkedIn(graph, ev, node.id, p->id, want, fallback);
}

float inF(const rigkit::ecs::NodeGraphData& graph, const EvalResult& ev,
		  const rigkit::ecs::GraphNode& node, std::string_view name, float fallback) {
	return inPin(graph, ev, node, name, "float", glm::vec4(fallback)).x;
}

void setOut(EvalResult& ev, const rigkit::ecs::GraphNode& node, const glm::vec4& v) {
	const auto* out = pinByName(node, "out", rigkit::ecs::NodePinKind::Out);
	if (out) {
		ev.pinValue[pinKey(node.id, out->id)] = v;
	}
}

void setOutF(EvalResult& ev, const rigkit::ecs::GraphNode& node, float v) {
	setOut(ev, node, glm::vec4(v));
}

float hash11(float n) {
	const float x = std::sin(n * 127.1f) * 43758.5453f;
	return x - std::floor(x);
}

float valueNoise(float t, float seed) {
	const float i = std::floor(t);
	const float f = t - i;
	const float u = f * f * (3.f - 2.f * f);
	const float a = hash11(i + seed * 19.19f);
	const float b = hash11(i + 1.f + seed * 19.19f);
	return a + (b - a) * u;
}

float lfoWave(float phase01, int wave) {
	const float p = phase01 - std::floor(phase01);
	switch (wave) {
	case 1:
		return 1.f - std::fabs(p * 4.f - 2.f);
	case 2:
		return p * 2.f - 1.f;
	case 3:
		return p < 0.5f ? 1.f : -1.f;
	default:
		return std::sin(p * kTau);
	}
}

float& scratchSlot(EvalContext& ctx, uint32_t nodeId, uint32_t slot, float init) {
	static float s_dummy = 0.f;
	if (!ctx.scratch) {
		s_dummy = init;
		return s_dummy;
	}
	const uint32_t key = nodeId ^ (slot * 0x9e3779b9u);
	auto it = ctx.scratch->find(key);
	if (it == ctx.scratch->end()) {
		it = ctx.scratch->emplace(key, init).first;
	}
	return it->second;
}

} // namespace

EvalResult evaluateGraph(const rigkit::ecs::NodeGraphData& graph, const EvalContext& ctxIn,
						 const std::unordered_map<uint64_t, glm::vec4>& seed) {
	EvalContext ctx = ctxIn;
	EvalResult result;
	result.pinValue = seed;
	if (graph.nodes.empty()) {
		result.ok = true;
		return result;
	}

	std::unordered_map<uint32_t, int> indeg;
	std::unordered_map<uint32_t, std::vector<uint32_t>> outs;
	for (const auto& n : graph.nodes) {
		indeg[n.id] = 0;
	}
	for (const auto& l : graph.links) {
		if (!graph.findNode(l.fromNode) || !graph.findNode(l.toNode)) {
			continue;
		}
		outs[l.fromNode].push_back(l.toNode);
		indeg[l.toNode] += 1;
	}

	std::queue<uint32_t> q;
	for (const auto& [id, d] : indeg) {
		if (d == 0) {
			q.push(id);
		}
	}

	std::vector<uint32_t> order;
	order.reserve(graph.nodes.size());
	while (!q.empty()) {
		const uint32_t id = q.front();
		q.pop();
		order.push_back(id);
		for (uint32_t to : outs[id]) {
			if (--indeg[to] == 0) {
				q.push(to);
			}
		}
	}
	if (order.size() != graph.nodes.size()) {
		result.error = "cycle in graph";
		return result;
	}

	const float dt = std::max(ctx.dt, 0.f);

	for (uint32_t id : order) {
		const auto* nodePtr = graph.findNode(id);
		if (!nodePtr) {
			continue;
		}
		const auto& node = *nodePtr;
		const std::string_view t = node.typeId;
		// Numeric params double as input pins (see reflectParamPins in the
		// catalog): a connected pin overrides the knob value.
		auto pf = [&](std::string_view key, float def) {
			return inF(graph, result, node, key, getParamFloat(node, key, def));
		};

		if (node.isGroup()) {
			EvalContext nestedCtx = ctx;
			nestedCtx.collectOutputs = false;
			std::unordered_map<uint64_t, glm::vec4> nestedSeed;
			for (const auto& pub : node.publishes) {
				const auto* outer = graph.findPin(id, pub.pin);
				if (!outer || outer->kind != rigkit::ecs::NodePinKind::In) {
					continue;
				}
				// Unconnected outer inputs stay unseeded so interior defaults
				// (e.g. an Export In node's Default param) apply. Injected values
				// (this group nested inside another) still count as connected.
				if (!pinHasIncoming(graph, id, pub.pin) &&
					result.pinValue.find(pinKey(id, pub.pin)) == result.pinValue.end()) {
					continue;
				}
				nestedSeed[pinKey(pub.innerNode, pub.innerPin)] =
					linkedIn(graph, result, id, pub.pin, outer->type, {});
			}
			EvalResult nestedOut = evaluateGraph(*node.nested, nestedCtx, nestedSeed);
			if (!nestedOut.ok) {
				result.error = nestedOut.error.empty() ? "group eval failed" : nestedOut.error;
				return result;
			}
			for (const auto& pub : node.publishes) {
				const auto* outer = graph.findPin(id, pub.pin);
				if (!outer || outer->kind != rigkit::ecs::NodePinKind::Out) {
					continue;
				}
				const auto it = nestedOut.pinValue.find(pinKey(pub.innerNode, pub.innerPin));
				if (it != nestedOut.pinValue.end()) {
					result.pinValue[pinKey(id, pub.pin)] = coerce(it->second, {}, outer->type);
				}
			}
			continue;
		}

		if (t == "graph.input") {
			// Group interface: the parent graph seeds this node's Out pin. When
			// unseeded (top level / unconnected outer pin) the Default param applies.
			for (const auto& p : node.pins) {
				if (p.kind == rigkit::ecs::NodePinKind::Out) {
					const uint64_t key = pinKey(id, p.id);
					if (result.pinValue.find(key) == result.pinValue.end()) {
						result.pinValue[key] = glm::vec4(getParamFloat(node, "value", 0.f));
					}
					break;
				}
			}
		} else if (t == "graph.output") {
			// Group interface: capture the In value so the parent can read it back.
			for (const auto& p : node.pins) {
				if (p.kind == rigkit::ecs::NodePinKind::In) {
					result.pinValue[pinKey(id, p.id)] =
						linkedIn(graph, result, id, p.id, p.type, {});
					break;
				}
			}
		} else if (t == "float.value") {
			setOutF(result, node, getParamFloat(node, "value", 0.f));
		} else if (t == "float.time") {
			setOutF(result, node, ctx.time);
		} else if (t == "float.dt") {
			setOutF(result, node, dt);
		} else if (t == "float.add") {
			setOutF(result, node,
					inF(graph, result, node, "a", 0.f) + inF(graph, result, node, "b", 0.f));
		} else if (t == "float.sub") {
			setOutF(result, node,
					inF(graph, result, node, "a", 0.f) - inF(graph, result, node, "b", 0.f));
		} else if (t == "float.mul") {
			setOutF(result, node,
					inF(graph, result, node, "a", 0.f) * inF(graph, result, node, "b", 1.f));
		} else if (t == "float.div") {
			const float b = inF(graph, result, node, "b", 1.f);
			setOutF(result, node,
					inF(graph, result, node, "a", 0.f) / (std::fabs(b) < 1e-8f ? 1e-8f : b));
		} else if (t == "float.mod") {
			const float b = inF(graph, result, node, "b", 1.f);
			setOutF(result, node,
					(std::fabs(b) < 1e-8f) ? 0.f
										   : std::fmod(inF(graph, result, node, "a", 0.f), b));
		} else if (t == "float.pow") {
			setOutF(
				result, node,
				std::pow(inF(graph, result, node, "a", 0.f), inF(graph, result, node, "b", 1.f)));
		} else if (t == "float.min") {
			setOutF(
				result, node,
				std::min(inF(graph, result, node, "a", 0.f), inF(graph, result, node, "b", 0.f)));
		} else if (t == "float.max") {
			setOutF(
				result, node,
				std::max(inF(graph, result, node, "a", 0.f), inF(graph, result, node, "b", 0.f)));
		} else if (t == "float.abs") {
			setOutF(result, node, std::fabs(inF(graph, result, node, "in", 0.f)));
		} else if (t == "float.neg") {
			setOutF(result, node, -inF(graph, result, node, "in", 0.f));
		} else if (t == "float.floor") {
			setOutF(result, node, std::floor(inF(graph, result, node, "in", 0.f)));
		} else if (t == "float.ceil") {
			setOutF(result, node, std::ceil(inF(graph, result, node, "in", 0.f)));
		} else if (t == "float.fract") {
			const float v = inF(graph, result, node, "in", 0.f);
			setOutF(result, node, v - std::floor(v));
		} else if (t == "float.sin") {
			setOutF(result, node, std::sin(inF(graph, result, node, "in", 0.f)));
		} else if (t == "float.cos") {
			setOutF(result, node, std::cos(inF(graph, result, node, "in", 0.f)));
		} else if (t == "float.tan") {
			setOutF(result, node, std::tan(inF(graph, result, node, "in", 0.f)));
		} else if (t == "float.clamp") {
			const float v = inF(graph, result, node, "a", 0.f);
			const float lo = inF(graph, result, node, "b", getParamFloat(node, "min", 0.f));
			const float hi = inF(graph, result, node, "c", getParamFloat(node, "max", 1.f));
			setOutF(result, node, std::clamp(v, std::min(lo, hi), std::max(lo, hi)));
		} else if (t == "float.map") {
			const float v = inF(graph, result, node, "in", 0.f);
			const float inMin = pf("inMin", 0.f);
			const float inMax = pf("inMax", 1.f);
			const float outMin = pf("outMin", 0.f);
			const float outMax = pf("outMax", 1.f);
			const float denom = inMax - inMin;
			const float n = (std::fabs(denom) < 1e-8f) ? 0.f : (v - inMin) / denom;
			setOutF(result, node, outMin + n * (outMax - outMin));
		} else if (t == "float.mix") {
			const float a = inF(graph, result, node, "a", 0.f);
			const float b = inF(graph, result, node, "b", 1.f);
			const float m = inF(graph, result, node, "c", 0.5f);
			setOutF(result, node, a + (b - a) * m);
		} else if (t == "float.step") {
			setOutF(result, node,
					inF(graph, result, node, "b", 0.f) < inF(graph, result, node, "a", 0.5f) ? 0.f
																							 : 1.f);
		} else if (t == "float.smoothstep") {
			const float e0 = inF(graph, result, node, "a", 0.f);
			const float e1 = inF(graph, result, node, "b", 1.f);
			const float x = inF(graph, result, node, "c", 0.5f);
			const float tn =
				(std::fabs(e1 - e0) < 1e-8f) ? 0.f : std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
			setOutF(result, node, tn * tn * (3.f - 2.f * tn));
		} else if (t == "mod.lfo") {
			const float freq = pf("freq", 1.f);
			const float amp = pf("amp", 1.f);
			const float offset = pf("offset", 0.f);
			const float phase = pf("phase", 0.f);
			const int wave = static_cast<int>(getParamFloat(node, "wave", 0.f));
			setOutF(result, node, offset + amp * lfoWave(ctx.time * freq + phase, wave));
		} else if (t == "mod.noise") {
			const float freq = pf("freq", 1.f);
			const float amp = pf("amp", 1.f);
			const float offset = pf("offset", 0.f);
			const float seed = pf("seed", 0.f);
			setOutF(result, node, offset + amp * (valueNoise(ctx.time * freq, seed) * 2.f - 1.f));
		} else if (t == "mod.random") {
			const float rate = std::max(0.01f, pf("rate", 2.f));
			const float lo = pf("min", 0.f);
			const float hi = pf("max", 1.f);
			const float seed = pf("seed", 1.f);
			float& held = scratchSlot(ctx, id, 0, lo);
			float& nextT = scratchSlot(ctx, id, 1, ctx.time);
			if (ctx.time >= nextT) {
				held = lo + hash11(std::floor(ctx.time * rate) + seed * 13.37f) * (hi - lo);
				nextT = ctx.time + 1.f / rate;
			}
			setOutF(result, node, held);
		} else if (t == "mod.ramp") {
			const float period = std::max(0.01f, pf("period", 2.f));
			const float from = pf("from", 0.f);
			const float to = pf("to", 1.f);
			const float u = (ctx.time / period) - std::floor(ctx.time / period);
			setOutF(result, node, from + (to - from) * u);
		} else if (t == "mod.envelope") {
			const float gate = inF(graph, result, node, "gate", 0.f);
			const float attack = std::max(0.001f, pf("attack", 0.05f));
			const float decay = std::max(0.001f, pf("decay", 0.1f));
			const float sustain = std::clamp(pf("sustain", 0.7f), 0.f, 1.f);
			const float release = std::max(0.001f, pf("release", 0.2f));
			float& level = scratchSlot(ctx, id, 0, 0.f);
			float& stage = scratchSlot(ctx, id, 1, 0.f);
			const bool on = gate >= 0.5f;
			if (on && stage < 1.f) {
				stage = 1.f;
			}
			if (!on && stage >= 1.f && stage < 4.f) {
				stage = 4.f;
			}
			if (stage < 1.f) {
				level = 0.f;
			} else if (stage < 2.f) {
				level += dt / attack;
				if (level >= 1.f) {
					level = 1.f;
					stage = 2.f;
				}
			} else if (stage < 3.f) {
				level -= dt * (1.f - sustain) / decay;
				if (level <= sustain) {
					level = sustain;
					stage = 3.f;
				}
			} else if (stage < 4.f) {
				level = sustain;
			} else {
				level -= dt / release;
				if (level <= 0.f) {
					level = 0.f;
					stage = 0.f;
				}
			}
			setOutF(result, node, std::clamp(level, 0.f, 1.f));
		} else if (t == "mod.beat") {
			const TempoBusValues tempo = getTempoBus();
			const float beats = std::max(0.01f, pf("beats", 1.f));
			const float pw = std::clamp(pf("pulseW", 0.1f), 0.01f, 0.99f);
			const float fallbackBpm = std::max(1.f, pf("bpm", 120.f));
			// No clock source -> free-run from eval time at the BPM knob.
			const double beat =
				tempo.active ? tempo.beat : static_cast<double>(ctx.time) * fallbackBpm / 60.0;
			const float bpb = tempo.active ? std::max(1.f, tempo.beatsPerBar) : 4.f;
			const double cycle = beat / static_cast<double>(beats);
			const double barCycle = beat / static_cast<double>(bpb);
			const float phase = static_cast<float>(cycle - std::floor(cycle));
			auto putOut = [&](const char* name, float v) {
				if (const auto* p = pinByName(node, name, rigkit::ecs::NodePinKind::Out)) {
					result.pinValue[pinKey(id, p->id)] = glm::vec4(v);
				}
			};
			putOut("phase", phase);
			putOut("bar", static_cast<float>(barCycle - std::floor(barCycle)));
			putOut("pulse", phase < pw ? 1.f : 0.f);
			putOut("bpm", tempo.active ? tempo.bpm : fallbackBpm);
		} else if (t == "mod.audio") {
			const AudioBusValues bus = getAudioBus();
			const float gain = pf("gain", 1.f);
			const float attack = std::max(0.001f, pf("attack", 0.02f));
			const float release = std::max(0.001f, pf("release", 0.25f));
			auto follow = [&](const char* pinName, uint32_t slot, float target) {
				float& s = scratchSlot(ctx, id, slot, 0.f);
				const float tc = (target > s) ? attack : release;
				s += (target - s) * (1.f - std::exp(-dt / tc));
				if (const auto* p = pinByName(node, pinName, rigkit::ecs::NodePinKind::Out)) {
					result.pinValue[pinKey(id, p->id)] = glm::vec4(s * gain);
				}
			};
			follow("level", 0, bus.active ? bus.level : 0.f);
			follow("bass", 1, bus.active ? bus.bass : 0.f);
			follow("mid", 2, bus.active ? bus.mid : 0.f);
			follow("treble", 3, bus.active ? bus.treble : 0.f);
		} else if (t == "mod.cv") {
			const CvBusValues cv = getCvBus();
			const int ch = std::clamp(static_cast<int>(pf("channel", 1.f)) - 1, 0,
									  CvBusValues::kMaxChannels - 1);
			const float raw = (cv.active && ch < cv.channels) ? cv.value[ch] : 0.f;
			const float target = raw * pf("scale", 1.f) + pf("offset", 0.f);
			const float slew = pf("slew", 0.005f);
			float& s = scratchSlot(ctx, id, 0, target);
			if (slew <= 0.f) {
				s = target;
			} else {
				s += (target - s) * (1.f - std::exp(-dt / slew));
			}
			setOutF(result, node, s);
		} else if (t == "midi.in") {
			const MidiInValues bus = getMidiIn();
			const int wantCh = static_cast<int>(pf("ch", 0.f));
			const bool match = bus.active && (wantCh <= 0 || wantCh == bus.channel);
			const int ccNum = std::clamp(static_cast<int>(pf("ccNum", 1.f)), 0, 127);
			auto putOut = [&](const char* name, float v) {
				if (const auto* p = pinByName(node, name, rigkit::ecs::NodePinKind::Out)) {
					result.pinValue[pinKey(id, p->id)] = glm::vec4(v);
				}
			};
			putOut("gate", match && bus.gate ? 1.f : 0.f);
			putOut("note", match ? bus.note : 0.f);
			putOut("vel", match ? bus.vel : 0.f);
			putOut("pulse", match && bus.noteOn ? 1.f : 0.f);
			putOut("cc", match ? bus.cc[ccNum] : 0.f);
		} else if (t == "midi.note") {
			const float gate = inF(graph, result, node, "gate", pf("gate", 0.f));
			const int note = std::clamp(static_cast<int>(pf("note", 60.f)), 0, 127);
			const int vel = std::clamp(static_cast<int>(pf("vel", 100.f)), 1, 127);
			const int ch = std::clamp(static_cast<int>(pf("ch", 1.f)), 1, 16);
			float& prevGate = scratchSlot(ctx, id, 0, 0.f);
			float& heldNote = scratchSlot(ctx, id, 1, -1.f);
			const bool on = gate >= 0.5f;
			const bool was = prevGate >= 0.5f;
			const uint8_t statusOn = static_cast<uint8_t>(0x90 | (ch - 1));
			const uint8_t statusOff = static_cast<uint8_t>(0x80 | (ch - 1));
			if (on && !was) {
				pushMidiOut(statusOn, static_cast<uint8_t>(note), static_cast<uint8_t>(vel));
				heldNote = static_cast<float>(note);
			} else if (on && was && static_cast<int>(heldNote) != note && heldNote >= 0.f) {
				pushMidiOut(statusOff, static_cast<uint8_t>(heldNote), 0);
				pushMidiOut(statusOn, static_cast<uint8_t>(note), static_cast<uint8_t>(vel));
				heldNote = static_cast<float>(note);
			} else if (!on && was && heldNote >= 0.f) {
				pushMidiOut(statusOff, static_cast<uint8_t>(heldNote), 0);
				heldNote = -1.f;
			}
			prevGate = on ? 1.f : 0.f;
			if (const auto* p = pinByName(node, "on", rigkit::ecs::NodePinKind::Out)) {
				result.pinValue[pinKey(id, p->id)] = glm::vec4(on && !was ? 1.f : 0.f);
			}
		} else if (t == "midi.cc") {
			const float value =
				std::clamp(inF(graph, result, node, "value", pf("value", 0.f)), 0.f, 1.f);
			const int cc = std::clamp(static_cast<int>(pf("cc", 1.f)), 0, 127);
			const int ch = std::clamp(static_cast<int>(pf("ch", 1.f)), 1, 16);
			const int sent = static_cast<int>(std::lround(value * 127.f));
			float& last = scratchSlot(ctx, id, 0, -1.f);
			if (static_cast<int>(last) != sent) {
				pushMidiOut(static_cast<uint8_t>(0xB0 | (ch - 1)), static_cast<uint8_t>(cc),
							static_cast<uint8_t>(sent));
				last = static_cast<float>(sent);
			}
			setOutF(result, node, value);
		} else if (t == "logic.compare") {
			const float a = inF(graph, result, node, "a", 0.f);
			const float b = inF(graph, result, node, "b", 0.f);
			const int op = static_cast<int>(getParamFloat(node, "op", 0.f));
			const float eps = pf("epsilon", 0.0001f);
			bool ok = false;
			switch (op) {
			case 1:
				ok = a <= b + eps;
				break;
			case 2:
				ok = a > b;
				break;
			case 3:
				ok = a >= b - eps;
				break;
			case 4:
				ok = std::fabs(a - b) <= eps;
				break;
			case 5:
				ok = std::fabs(a - b) > eps;
				break;
			default:
				ok = a < b;
				break;
			}
			setOutF(result, node, ok ? 1.f : 0.f);
		} else if (t == "logic.gate") {
			setOutF(result, node,
					inF(graph, result, node, "gate", 0.f) >= 0.5f
						? inF(graph, result, node, "in", 0.f)
						: 0.f);
		} else if (t == "logic.switch") {
			setOutF(result, node,
					inF(graph, result, node, "sel", 0.f) >= 0.5f
						? inF(graph, result, node, "b", 0.f)
						: inF(graph, result, node, "a", 0.f));
		} else if (t == "vec2.value") {
			setOut(
				result, node,
				glm::vec4(getParamFloat(node, "x", 0.f), getParamFloat(node, "y", 0.f), 0.f, 0.f));
		} else if (t == "vec2.split") {
			const glm::vec4 v = inPin(graph, result, node, "in", "vec2", {});
			if (const auto* x = pinByName(node, "x", rigkit::ecs::NodePinKind::Out)) {
				result.pinValue[pinKey(id, x->id)] = glm::vec4(v.x);
			}
			if (const auto* y = pinByName(node, "y", rigkit::ecs::NodePinKind::Out)) {
				result.pinValue[pinKey(id, y->id)] = glm::vec4(v.y);
			}
		} else if (t == "vec2.join") {
			setOut(result, node,
				   glm::vec4(inF(graph, result, node, "x", 0.f), inF(graph, result, node, "y", 0.f),
							 0.f, 0.f));
		} else if (t == "vec2.add") {
			setOut(result, node,
				   inPin(graph, result, node, "a", "vec2", {}) +
					   inPin(graph, result, node, "b", "vec2", {}));
		} else if (t == "vec2.sub") {
			setOut(result, node,
				   inPin(graph, result, node, "a", "vec2", {}) -
					   inPin(graph, result, node, "b", "vec2", {}));
		} else if (t == "vec2.mul") {
			setOut(result, node,
				   inPin(graph, result, node, "a", "vec2", {}) *
					   inPin(graph, result, node, "b", "vec2", glm::vec4(1.f)));
		} else if (t == "vec2.scale") {
			const glm::vec4 v = inPin(graph, result, node, "v", "vec2", {});
			const float s = inF(graph, result, node, "s", 1.f);
			setOut(result, node, v * s);
		} else if (t == "vec2.length") {
			const glm::vec4 v = inPin(graph, result, node, "in", "vec2", {});
			setOutF(result, node, std::sqrt(v.x * v.x + v.y * v.y));
		} else if (t == "vec2.normalize") {
			const glm::vec4 v = inPin(graph, result, node, "in", "vec2", {});
			const float len = std::sqrt(v.x * v.x + v.y * v.y);
			setOut(result, node,
				   len < 1e-8f ? glm::vec4(0.f) : glm::vec4(v.x / len, v.y / len, 0.f, 0.f));
		} else if (t == "vec2.dot") {
			const glm::vec4 a = inPin(graph, result, node, "a", "vec2", {});
			const glm::vec4 b = inPin(graph, result, node, "b", "vec2", {});
			setOutF(result, node, a.x * b.x + a.y * b.y);
		} else if (t == "color.value") {
			setOut(result, node,
				   glm::vec4(getParamFloat(node, "r", 1.f), getParamFloat(node, "g", 1.f),
							 getParamFloat(node, "b", 1.f), getParamFloat(node, "a", 1.f)));
		} else if (t == "color.split") {
			const glm::vec4 v = inPin(graph, result, node, "in", "vec4", glm::vec4(1.f));
			auto put = [&](const char* name, float c) {
				if (const auto* p = pinByName(node, name, rigkit::ecs::NodePinKind::Out)) {
					result.pinValue[pinKey(id, p->id)] = glm::vec4(c);
				}
			};
			put("r", v.r);
			put("g", v.g);
			put("b", v.b);
			put("a", v.a);
		} else if (t == "color.join") {
			setOut(result, node,
				   glm::vec4(inF(graph, result, node, "r", 1.f), inF(graph, result, node, "g", 1.f),
							 inF(graph, result, node, "b", 1.f),
							 inF(graph, result, node, "a", 1.f)));
		} else if (t == "color.mix") {
			const glm::vec4 a =
				inPin(graph, result, node, "a", "vec4", glm::vec4(0.f, 0.f, 0.f, 1.f));
			const glm::vec4 b = inPin(graph, result, node, "b", "vec4", glm::vec4(1.f));
			const float m = inF(graph, result, node, "t", 0.5f);
			setOut(result, node, a + (b - a) * m);
		} else if (t == "color.brightness") {
			const glm::vec4 c = inPin(graph, result, node, "in", "vec4", glm::vec4(1.f));
			const float g = inF(graph, result, node, "gain", 1.f);
			setOut(result, node, glm::vec4(c.r * g, c.g * g, c.b * g, c.a));
		} else if (t == "float.out") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const float v = in ? linkedIn(graph, result, id, in->id, "float", {}).x : 0.f;
			if (in) {
				result.pinValue[pinKey(id, in->id)] = glm::vec4(v);
			}
			if (ctx.collectOutputs) {
				result.outputs.push_back(v);
			}
		} else if (t == "vec2.out") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const glm::vec4 v =
				in ? linkedIn(graph, result, id, in->id, "vec2", {}) : glm::vec4(0.f);
			if (in) {
				result.pinValue[pinKey(id, in->id)] = v;
			}
			if (ctx.collectOutputs) {
				result.vec2Outputs.push_back(glm::vec2(v.x, v.y));
			}
		} else if (t == "color.out") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const glm::vec4 v =
				in ? linkedIn(graph, result, id, in->id, "vec4", glm::vec4(0.f, 0.f, 0.f, 1.f))
				   : glm::vec4(0.f, 0.f, 0.f, 1.f);
			if (in) {
				result.pinValue[pinKey(id, in->id)] = v;
			}
			if (ctx.collectOutputs) {
				result.colorOutputs.push_back(v);
			}
		} else if (t == "ref.float") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const float v = in ? linkedIn(graph, result, id, in->id, "float", {}).x : 0.f;
			if (in) {
				result.pinValue[pinKey(id, in->id)] = glm::vec4(v);
			}
			// Orphaned refs must not write (delete LFO must stop driving).
			if (ctx.collectOutputs && in && pinHasIncoming(graph, id, in->id)) {
				RefWrite w;
				w.entity = getParamString(node, "entity");
				w.prop = getParamString(node, "prop");
				w.kind = RefWriteKind::Float;
				w.value = glm::vec4(v);
				result.refWrites.push_back(std::move(w));
			}
		} else if (t == "ref.vec2") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const glm::vec4 v =
				in ? linkedIn(graph, result, id, in->id, "vec2", {}) : glm::vec4(0.f);
			if (in) {
				result.pinValue[pinKey(id, in->id)] = v;
			}
			if (ctx.collectOutputs && in && pinHasIncoming(graph, id, in->id)) {
				RefWrite w;
				w.entity = getParamString(node, "entity");
				w.prop = getParamString(node, "prop");
				w.propY = getParamString(node, "propY");
				w.kind = RefWriteKind::Vec2;
				w.value = v;
				result.refWrites.push_back(std::move(w));
			}
		} else if (t == "ref.color") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const glm::vec4 v =
				in ? linkedIn(graph, result, id, in->id, "vec4", glm::vec4(0.f, 0.f, 0.f, 1.f))
				   : glm::vec4(0.f, 0.f, 0.f, 1.f);
			if (in) {
				result.pinValue[pinKey(id, in->id)] = v;
			}
			if (ctx.collectOutputs && in && pinHasIncoming(graph, id, in->id)) {
				RefWrite w;
				w.entity = getParamString(node, "entity");
				w.prop = getParamString(node, "prop");
				w.kind = RefWriteKind::Color;
				w.value = v;
				result.refWrites.push_back(std::move(w));
			}
		}
	}

	result.ok = true;
	return result;
}

EvalResult evaluateAlongDive(const rigkit::ecs::NodeGraphData& root,
							 const std::vector<uint32_t>& divePath, const EvalContext& ctx) {
	if (divePath.empty()) {
		return evaluateGraph(root, ctx);
	}
	const rigkit::ecs::NodeGraphData* g = &root;
	EvalResult parentEv = evaluateGraph(*g, ctx);
	if (!parentEv.ok) {
		return parentEv;
	}
	for (size_t depth = 0; depth < divePath.size(); ++depth) {
		const uint32_t id = divePath[depth];
		const auto* node = g->findNode(id);
		if (!node || !node->nested) {
			parentEv.error = "dive path broken";
			parentEv.ok = false;
			return parentEv;
		}
		std::unordered_map<uint64_t, glm::vec4> seed;
		for (const auto& pub : node->publishes) {
			const auto* outer = g->findPin(id, pub.pin);
			if (!outer || outer->kind != rigkit::ecs::NodePinKind::In) {
				continue;
			}
			if (!pinHasIncoming(*g, id, pub.pin) &&
				parentEv.pinValue.find(pinKey(id, pub.pin)) == parentEv.pinValue.end()) {
				continue;
			}
			seed[pinKey(pub.innerNode, pub.innerPin)] =
				linkedIn(*g, parentEv, id, pub.pin, outer->type, {});
		}
		EvalContext nestedCtx = ctx;
		nestedCtx.collectOutputs = (depth + 1 == divePath.size());
		parentEv = evaluateGraph(*node->nested, nestedCtx, seed);
		if (!parentEv.ok) {
			return parentEv;
		}
		g = node->nested.get();
	}
	return parentEv;
}

} // namespace node
} // namespace rig
