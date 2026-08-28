#include "rigNodeComponent.h"

#include "core/pack/MPack.h"
#include "core/pack/PackRegistry.h"
#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include "ecs/SystemRegistry.h"
#include "CNodeGraph.h"
#include "NodeSerializers.h"
#include "rigProject.h"
#include "SGraphEval.h"

#include <spdlog/spdlog.h>

namespace rigkit {

rigNodeComponent::rigNodeComponent() : IPack("rigNodeComponent") {
	

}

bool rigNodeComponent::init() {
	spdlog::info("[rigNodeComponent] init (data-only)");
	return true;
}

void rigNodeComponent::setup() {
	auto* engine = getEngine();
	if (!engine) {
		return;
	}
	auto* ecs = engine->getECSManager();
	if (!ecs) {
		return;
	}
	ecs->registerComponent<ecs::CNodeGraph>("NodeGraph", true);
	ecs->registerSystem("SGraphEval", SystemPhase::Update, ecs::SGraphEval);

	if (auto* packs = engine->getPackManager()) {
		if (auto doc = std::dynamic_pointer_cast<rigProject>(packs->getPack("rigProject"))) {
			node::registerProjectSerializers(doc->serializer().registry());
			spdlog::info("[rigNodeComponent] registered NodeGraph codecs on rigProject");
		} else {
			spdlog::warn("[rigNodeComponent] rigProject not loaded - .rig codecs skipped");
		}
	}

	spdlog::info("[rigNodeComponent] registered CNodeGraph");
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
