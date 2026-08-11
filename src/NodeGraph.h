#pragma once

#include "CNodeGraph.h"
#include "ecs/MEcs.h"

#include <string>
#include <string_view>
#include <vector>

namespace rig {
namespace node {

struct PinDesc {
	std::string name;
	rigkit::ecs::NodePinKind kind = rigkit::ecs::NodePinKind::In;
	/// Rig property datatype id (`float`, `vec2`, `vec4`, …). Empty = wildcard.
	std::string type = "float";
};

/**
 * @brief Create an entity with an empty `CNodeGraph`.
 */
entt::entity makeGraph(rigkit::MEcs& ecs, std::string_view name = "node-graph");

/**
 * @brief Append a node; returns its id (0 on failure).
 */
uint32_t addNode(rigkit::ecs::NodeGraphData& graph, std::string_view typeId, std::string_view title,
				 glm::vec2 pos, const std::vector<PinDesc>& pins);

bool tryLink(rigkit::ecs::NodeGraphData& graph, uint32_t fromNode, uint32_t fromPin, uint32_t toNode,
			 uint32_t toPin);

bool tryLinkByName(rigkit::ecs::NodeGraphData& graph, uint32_t fromNode, std::string_view fromPin,
				   uint32_t toNode, std::string_view toPin);

void removeNode(rigkit::ecs::NodeGraphData& graph, uint32_t nodeId);
void removeLink(rigkit::ecs::NodeGraphData& graph, uint32_t linkId);

/**
 * @brief Move `nodeIds` into a new group node; returns group id (0 on failure).
 * @details Crossing links become publishes on the group interface.
 */
uint32_t createGroup(rigkit::ecs::NodeGraphData& graph, const std::vector<uint32_t>& nodeIds,
					 glm::vec2 pos, std::string_view title = "Group");

/**
 * @brief Publish an interior pin as an outer interface pin on a group node.
 * @return Outer pin id (0 on failure).
 */
uint32_t publishInnerPin(rigkit::ecs::GraphNode& group, uint32_t innerNode, uint32_t innerPin,
						 std::string_view outerName = {});

/** @brief Create an empty group node and return its id. */
uint32_t createEmptyGroup(rigkit::ecs::NodeGraphData& graph, glm::vec2 pos,
						  std::string_view title = "Group");

/**
 * @brief Flatten a group node back into `graph`. Returns false if not a group.
 * @details Rewires parent links through publishes onto interior pins.
 */
bool ungroup(rigkit::ecs::NodeGraphData& graph, uint32_t groupId);


float getParamFloat(const rigkit::ecs::GraphNode& node, std::string_view key, float fallback = 0.f);
void setParamFloat(rigkit::ecs::GraphNode& node, std::string_view key, float value,
				   std::string_view type = "float");

int getParamInt(const rigkit::ecs::GraphNode& node, std::string_view key, int fallback = 0);
void setParamInt(rigkit::ecs::GraphNode& node, std::string_view key, int value,
				 std::string_view type = "int");

glm::vec4 getParamVec4(const rigkit::ecs::GraphNode& node, std::string_view key,
					   const glm::vec4& fallback = {});
void setParamVec4(rigkit::ecs::GraphNode& node, std::string_view key, const glm::vec4& value,
				  std::string_view type = "vec4");

std::string getParamString(const rigkit::ecs::GraphNode& node, std::string_view key,
						   std::string_view fallback = {});
void setParamString(rigkit::ecs::GraphNode& node, std::string_view key, std::string_view value,
					std::string_view type = "string");

/**
 * @brief Seed Value → Add ← Value → Out demo graph (float catalog).
 * @details Value A = 1, Value B = 2 (evaluates to 3).
 */
void seedDemoGraph(rigkit::ecs::CNodeGraph& graph);

/** @brief Normalize legacy pin type strings (`color`→`vec4`, `any`→empty). */
std::string normalizePinType(std::string_view type);

} // namespace node
} // namespace rig
