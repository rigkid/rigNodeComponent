#include "SGraphEval.h"

#include "ApplyRefs.h"
#include "CNodeGraph.h"
#include "GraphEval.h"

#include <cstdint>
#include <unordered_map>

namespace rigkit {
namespace ecs {
namespace {

float g_graphTime = 0.f;
std::unordered_map<std::uint32_t, std::unordered_map<std::uint32_t, float>> g_scratchByEntity;

} // namespace

void SGraphEval(MEcs& ecs, float dt) {
	g_graphTime += dt;
	for (auto e : ecs.view<ecs::CNodeGraph>()) {
		auto& graph = ecs.getComponent<ecs::CNodeGraph>(e);
		auto& scratch = g_scratchByEntity[static_cast<std::uint32_t>(e)];
		rig::node::EvalContext ctx;
		ctx.time = g_graphTime;
		ctx.dt = dt;
		ctx.scratch = &scratch;
		const auto ev = rig::node::evaluateGraph(graph, ctx);
		if (ev.ok) {
			rig::node::applyRefWrites(ecs, ev);
		}
	}
}

} // namespace ecs
} // namespace rigkit
