#pragma once

#include <cstddef>
#include <string_view>
#include <vector>
#include "NodeGraph.h"

namespace rig {
namespace node {

struct CatalogParam {
	const char* key = "";
	const char* label = "";
	float def = 0.f;
	float min = 0.f;
	float max = 1.f;
	float speed = 0.01f;
	/// 0 = drag float, 1 = int combo (use min..max as inclusive indices)
	int ui = 0;
	const char* const* comboLabels = nullptr; // size = (max-min)+1 when ui==1
	/// Rig property datatype id (`float`, `enum`, `string`, ...)
	const char* type = "float";
	/// Default when `type` is `string`.
	const char* strDef = "";
};

struct CatalogEntry {
	const char* typeId = "";
	const char* title = "";
	const char* category = ""; ///< Values / Maths / Modulators / Logic / Output
	std::vector<PinDesc> pins;
	std::vector<CatalogParam> params;
};

/**
 * @brief Built-in Pixile-style catalog (maths, modulators, logic).
 * @details Static table - apps can still spawn custom `typeId`s; unknown types
 * are ignored by the float evaluator.
 */
const std::vector<CatalogEntry>& builtinCatalog();

const CatalogEntry* findCatalogEntry(std::string_view typeId);

/// Unique category names in catalog order.
std::vector<const char*> catalogCategories();

/**
 * @brief Spawn a catalog node with default pins/params. Returns 0 if unknown.
 */
uint32_t spawnCatalogNode(rigkit::ecs::NodeGraphData& graph, std::string_view typeId, glm::vec2 pos);

/**
 * @brief Add catalog pins missing from existing nodes (recursive into groups).
 * @details Migration for graphs saved before params were reflected as input
 * pins. Only adds - never removes or re-orders. Editors call this per frame
 * next to `syncGroupInterfaces`.
 */
void syncCatalogPins(rigkit::ecs::NodeGraphData& graph);

} // namespace node
} // namespace rig
