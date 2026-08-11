#include "GraphEval.h"

#include "NodeGraph.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>

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

EvalResult evaluateGraphImpl(const rigkit::ecs::NodeGraphData& graph, const EvalContext& ctxIn,
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

		if (node.isGroup()) {
			EvalContext nestedCtx = ctx;
			nestedCtx.collectOutputs = false;
			std::unordered_map<uint64_t, glm::vec4> nestedSeed;
			for (const auto& pub : node.publishes) {
				const auto* outer = graph.findPin(id, pub.pin);
				if (!outer || outer->kind != rigkit::ecs::NodePinKind::In) {
					continue;
				}
				nestedSeed[pinKey(pub.innerNode, pub.innerPin)] =
					linkedIn(graph, result, id, pub.pin, outer->type, {});
			}
			EvalResult nestedOut = evaluateGraphImpl(*node.nested, nestedCtx, nestedSeed);
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

		if (t == "float.value") {
			setOutF(result, node, getParamFloat(node, "value", 0.f));
		} else if (t == "float.time") {
			setOutF(result, node, ctx.time);
		} else if (t == "float.dt") {
			setOutF(result, node, dt);
		} else if (t == "float.add") {
			setOutF(result, node, inF(graph, result, node, "a", 0.f) + inF(graph, result, node, "b", 0.f));
		} else if (t == "float.sub") {
			setOutF(result, node, inF(graph, result, node, "a", 0.f) - inF(graph, result, node, "b", 0.f));
		} else if (t == "float.mul") {
			setOutF(result, node, inF(graph, result, node, "a", 0.f) * inF(graph, result, node, "b", 1.f));
		} else if (t == "float.div") {
			const float b = inF(graph, result, node, "b", 1.f);
			setOutF(result, node, inF(graph, result, node, "a", 0.f) / (std::fabs(b) < 1e-8f ? 1e-8f : b));
		} else if (t == "float.mod") {
			const float b = inF(graph, result, node, "b", 1.f);
			setOutF(result, node,
					(std::fabs(b) < 1e-8f) ? 0.f : std::fmod(inF(graph, result, node, "a", 0.f), b));
		} else if (t == "float.pow") {
			setOutF(result, node,
					std::pow(inF(graph, result, node, "a", 0.f), inF(graph, result, node, "b", 1.f)));
		} else if (t == "float.min") {
			setOutF(result, node,
					std::min(inF(graph, result, node, "a", 0.f), inF(graph, result, node, "b", 0.f)));
		} else if (t == "float.max") {
			setOutF(result, node,
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
			const float inMin = getParamFloat(node, "inMin", 0.f);
			const float inMax = getParamFloat(node, "inMax", 1.f);
			const float outMin = getParamFloat(node, "outMin", 0.f);
			const float outMax = getParamFloat(node, "outMax", 1.f);
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
			const float tn = (std::fabs(e1 - e0) < 1e-8f)
								 ? 0.f
								 : std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
			setOutF(result, node, tn * tn * (3.f - 2.f * tn));
		} else if (t == "mod.lfo") {
			const float freq = getParamFloat(node, "freq", 1.f);
			const float amp = getParamFloat(node, "amp", 1.f);
			const float offset = getParamFloat(node, "offset", 0.f);
			const float phase = getParamFloat(node, "phase", 0.f);
			const int wave = static_cast<int>(getParamFloat(node, "wave", 0.f));
			setOutF(result, node, offset + amp * lfoWave(ctx.time * freq + phase, wave));
		} else if (t == "mod.noise") {
			const float freq = getParamFloat(node, "freq", 1.f);
			const float amp = getParamFloat(node, "amp", 1.f);
			const float offset = getParamFloat(node, "offset", 0.f);
			const float seed = getParamFloat(node, "seed", 0.f);
			setOutF(result, node, offset + amp * (valueNoise(ctx.time * freq, seed) * 2.f - 1.f));
		} else if (t == "mod.random") {
			const float rate = std::max(0.01f, getParamFloat(node, "rate", 2.f));
			const float lo = getParamFloat(node, "min", 0.f);
			const float hi = getParamFloat(node, "max", 1.f);
			const float seed = getParamFloat(node, "seed", 1.f);
			float& held = scratchSlot(ctx, id, 0, lo);
			float& nextT = scratchSlot(ctx, id, 1, ctx.time);
			if (ctx.time >= nextT) {
				held = lo + hash11(std::floor(ctx.time * rate) + seed * 13.37f) * (hi - lo);
				nextT = ctx.time + 1.f / rate;
			}
			setOutF(result, node, held);
		} else if (t == "mod.ramp") {
			const float period = std::max(0.01f, getParamFloat(node, "period", 2.f));
			const float from = getParamFloat(node, "from", 0.f);
			const float to = getParamFloat(node, "to", 1.f);
			const float u = (ctx.time / period) - std::floor(ctx.time / period);
			setOutF(result, node, from + (to - from) * u);
		} else if (t == "mod.envelope") {
			const float gate = inF(graph, result, node, "gate", 0.f);
			const float attack = std::max(0.001f, getParamFloat(node, "attack", 0.05f));
			const float decay = std::max(0.001f, getParamFloat(node, "decay", 0.1f));
			const float sustain = std::clamp(getParamFloat(node, "sustain", 0.7f), 0.f, 1.f);
			const float release = std::max(0.001f, getParamFloat(node, "release", 0.2f));
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
		} else if (t == "logic.compare") {
			const float a = inF(graph, result, node, "a", 0.f);
			const float b = inF(graph, result, node, "b", 0.f);
			const int op = static_cast<int>(getParamFloat(node, "op", 0.f));
			const float eps = getParamFloat(node, "epsilon", 0.0001f);
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
					inF(graph, result, node, "gate", 0.f) >= 0.5f ? inF(graph, result, node, "in", 0.f)
																  : 0.f);
		} else if (t == "logic.switch") {
			setOutF(result, node,
					inF(graph, result, node, "sel", 0.f) >= 0.5f ? inF(graph, result, node, "b", 0.f)
																 : inF(graph, result, node, "a", 0.f));
		} else if (t == "vec2.value") {
			setOut(result, node,
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
				   glm::vec4(inF(graph, result, node, "x", 0.f), inF(graph, result, node, "y", 0.f), 0.f,
							 0.f));
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
			setOut(result, node, len < 1e-8f ? glm::vec4(0.f) : glm::vec4(v.x / len, v.y / len, 0.f, 0.f));
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
							 inF(graph, result, node, "b", 1.f), inF(graph, result, node, "a", 1.f)));
		} else if (t == "color.mix") {
			const glm::vec4 a = inPin(graph, result, node, "a", "vec4", glm::vec4(0.f, 0.f, 0.f, 1.f));
			const glm::vec4 b = inPin(graph, result, node, "b", "vec4", glm::vec4(1.f));
			const float m = inF(graph, result, node, "t", 0.5f);
			setOut(result, node, a + (b - a) * m);
		} else if (t == "color.brightness") {
			const glm::vec4 c = inPin(graph, result, node, "in", "vec4", glm::vec4(1.f));
			const float g = inF(graph, result, node, "gain", 1.f);
			setOut(result, node, glm::vec4(c.r * g, c.g * g, c.b * g, c.a));
		} else if (t == "float.out") {
			const auto* in = pinByName(node, "in", rigkit::ecs::NodePinKind::In);
			const float v =
				in ? linkedIn(graph, result, id, in->id, "float", {}).x : 0.f;
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
			const float v =
				in ? linkedIn(graph, result, id, in->id, "float", {}).x : 0.f;
			if (in) {
				result.pinValue[pinKey(id, in->id)] = glm::vec4(v);
			}
			if (ctx.collectOutputs) {
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
			if (ctx.collectOutputs) {
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
			if (ctx.collectOutputs) {
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

} // namespace

EvalResult evaluateGraph(const rigkit::ecs::NodeGraphData& graph, const EvalContext& ctx,
						 const std::unordered_map<uint64_t, glm::vec4>& seed) {
	return evaluateGraphImpl(graph, ctx, seed);
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
			seed[pinKey(pub.innerNode, pub.innerPin)] =
				linkedIn(*g, parentEv, id, pub.pin, outer->type, {});
		}
		EvalContext nestedCtx = ctx;
		nestedCtx.collectOutputs = (depth + 1 == divePath.size());
		parentEv = evaluateGraphImpl(*node->nested, nestedCtx, seed);
		if (!parentEv.ok) {
			return parentEv;
		}
		g = node->nested.get();
	}
	return parentEv;
}

} // namespace node
} // namespace rig
