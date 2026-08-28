#include "ApplyRefs.h"
#include "EntityProperty.h"

namespace rig {
namespace node {

void applyRefWrites(rigkit::MEcs& ecs, const EvalResult& result) {
	for (const auto& w : result.refWrites) {
		if (w.entity.empty()) {
			continue;
		}
		const entt::entity e = ecs.findEntity(w.entity);
		if (e == entt::null) {
			continue;
		}
		if (w.kind == RefWriteKind::Float) {
			if (!w.prop.empty()) {
				rigkit::ecs::writeEntityProperty(ecs, e, w.prop, w.value.x);
			}
		} else if (w.kind == RefWriteKind::Vec2) {
			if (!w.prop.empty() && w.propY.empty()) {
				rigkit::ecs::writeEntityVec2Property(ecs, e, w.prop,
													 glm::vec2(w.value.x, w.value.y));
			} else {
				if (!w.prop.empty()) {
					rigkit::ecs::writeEntityProperty(ecs, e, w.prop, w.value.x);
				}
				if (!w.propY.empty()) {
					rigkit::ecs::writeEntityProperty(ecs, e, w.propY, w.value.y);
				}
			}
		} else if (w.kind == RefWriteKind::Color) {
			const std::string r = w.prop.empty() ? "Color R" : w.prop + " R";
			const std::string g = w.prop.empty() ? "Color G" : w.prop + " G";
			const std::string b = w.prop.empty() ? "Color B" : w.prop + " B";
			rigkit::ecs::writeEntityProperty(ecs, e, r, w.value.r);
			rigkit::ecs::writeEntityProperty(ecs, e, g, w.value.g);
			rigkit::ecs::writeEntityProperty(ecs, e, b, w.value.b);
		}
	}
}

} // namespace node
} // namespace rig
