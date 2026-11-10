#include "rigNodeComponent.h"

#include "core/RigKitEngine.h"
#include "core/pack/PackRegistry.h"
#include "ecs/MEcs.h"
#include "CNodeGraph.h"

#include <spdlog/spdlog.h>

namespace rigkit {

rigNodeComponent::rigNodeComponent() : IPack("rigNodeComponent") {}

bool rigNodeComponent::init() {
	spdlog::info("[rigNodeComponent] init (node graph data)");
	return true;
}

void rigNodeComponent::setup() {
	auto* engine = getEngine();
	if (!engine) {
		return;
	}
	if (auto* ecs = engine->getECSManager()) {
		ecs->registerComponent<ecs::CNodeGraph>("CNodeGraph");
		spdlog::info("[rigNodeComponent] registered CNodeGraph");
	}
}

} // namespace rigkit

namespace {
struct rigNodeComponentRegistrar {
	rigNodeComponentRegistrar() {
		rigkit::PackRegistry::instance().addFactory("rigNodeComponent", []() {
			return std::shared_ptr<rigkit::IPack>(std::make_shared<rigkit::rigNodeComponent>());
		});
	}
};
static rigNodeComponentRegistrar rigNodeComponent_auto_reg;
} // namespace
