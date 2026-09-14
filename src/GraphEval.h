#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include "CNodeGraph.h"

namespace rig {
namespace node {

struct EvalContext {
	float time = 0.f;
	float dt = 1.f / 60.f;
	/// Optional per-node scratch (envelope / random hold).
	std::unordered_map<uint32_t, float>* scratch = nullptr;
	/// When false (nested group eval), skip float.out / vec2.out / color.out lists.
	bool collectOutputs = true;
};

enum class RefWriteKind : uint8_t { Float = 0, Vec2 = 1, Color = 2 };

/// Intent collected by `ref.*` sinks - apply with `applyRefWrites`.
struct RefWrite {
	std::string entity;
	std::string prop;
	std::string propY; ///< Optional second float prop for `ref.vec2`.
	RefWriteKind kind = RefWriteKind::Float;
	glm::vec4 value{0.f};
};

struct EvalResult {
	bool ok = false;
	std::string error;
	/// Pin values as vec4 (float uses .x; vec2 uses .xy; colour uses .rgba).
	std::unordered_map<uint64_t, glm::vec4> pinValue;
	std::vector<float> outputs;
	std::vector<glm::vec2> vec2Outputs;
	std::vector<glm::vec4> colorOutputs;
	std::vector<RefWrite> refWrites;
};

inline uint64_t pinKey(uint32_t nodeId, uint32_t pinId) {
	return (static_cast<uint64_t>(nodeId) << 32) | static_cast<uint64_t>(pinId);
}

/**
 * @brief Evaluate a graph body (root or nested). Pin types use Rig property datatypes.
 * @details Group nodes (`nested` set) recurse. Unknown leaf `typeId`s are skipped.
 * Optional `seed` injects pin values (used for published group inputs / dive preview).
 */
EvalResult evaluateGraph(const rigkit::ecs::NodeGraphData& graph, const EvalContext& ctx = {},
						 const std::unordered_map<uint64_t, glm::vec4>& seed = {});

inline EvalResult evaluateGraph(const rigkit::ecs::CNodeGraph& graph, const EvalContext& ctx = {}) {
	return evaluateGraph(static_cast<const rigkit::ecs::NodeGraphData&>(graph), ctx);
}

/**
 * @brief Evaluate the graph at the end of a dive path, seeding each level from parent publishes.
 * @details `divePath` is group node ids from root downward. Empty path = evaluate `root`.
 */
EvalResult evaluateAlongDive(const rigkit::ecs::NodeGraphData& root,
							 const std::vector<uint32_t>& divePath, const EvalContext& ctx = {});

} // namespace node
} // namespace rig
