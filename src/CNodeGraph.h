#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ecs/PropertyReflection.h"

namespace rigkit {
namespace ecs {

enum class NodePinKind : uint8_t { In = 0, Out = 1 };

/**
 * @brief One pin on a graph node (input or output).
 * @details Speaks `rig.node.pin`. `type` is a Rig property datatype id.
 */
struct NodePin {
	uint32_t id = 0;
	std::string name;
	NodePinKind kind = NodePinKind::In;
	std::string type = "float";
};

/**
 * @brief Inline property row on a node. Speaks `rig.node.param`.
 */
struct NodeParam {
	std::string key;
	std::string type = "float";
	float f = 0.f;
	int i = 0;
	std::string s;
	glm::vec4 v{0.f, 0.f, 0.f, 0.f};
};

/**
 * @brief Outer pin → interior pin. Speaks `rig.node.publish`.
 */
struct NodePublish {
	uint32_t pin = 0;
	uint32_t innerNode = 0;
	uint32_t innerPin = 0;
};

struct GraphNode;
struct GraphLink;

/**
 * @brief Graph body (nodes + links). Used as root `CNodeGraph` and as `nested`.
 */
struct NodeGraphData {
	std::vector<GraphNode> nodes;
	std::vector<GraphLink> links;
	uint32_t nextId = 1;

	uint32_t allocId() { return nextId++; }

	GraphNode* findNode(uint32_t id);
	const GraphNode* findNode(uint32_t id) const;
	GraphLink* findLink(uint32_t id);
	const NodePin* findPin(uint32_t nodeId, uint32_t pinId) const;
};

struct GraphLink {
	uint32_t id = 0;
	uint32_t fromNode = 0;
	uint32_t fromPin = 0;
	uint32_t toNode = 0;
	uint32_t toPin = 0;
};

/**
 * @brief One node in a graph. Speaks `rig.node.node`.
 * @details When `nested` is non-null, this node is a group; `publishes` maps
 * outer pins to interior pins. Groups nest recursively.
 */
struct GraphNode {
	uint32_t id = 0;
	std::string typeId;
	std::string title;
	glm::vec2 pos{0.f, 0.f};
	std::vector<NodePin> pins;
	std::vector<NodeParam> params;
	std::unique_ptr<NodeGraphData> nested;
	std::vector<NodePublish> publishes;

	GraphNode();
	~GraphNode();
	GraphNode(const GraphNode& other);
	GraphNode& operator=(const GraphNode& other);
	GraphNode(GraphNode&&) noexcept;
	GraphNode& operator=(GraphNode&&) noexcept;

	bool isGroup() const { return nested != nullptr; }
};

/**
 * @brief Portable node graph document (ECS component).
 * @details Speaks `rig.node.graph`.
 * `editSelectedNode` / `editDivePath` are editor chrome (not serialized).
 */
struct CNodeGraph : NodeGraphData {
	int propNodeCount = 0;
	int propLinkCount = 0;
	int propNextId = 1;
	/// Node Editor → Properties: selected node id in the active dive level.
	uint32_t editSelectedNode = 0;
	/// Group dive path (root → … → current nested graph).
	std::vector<uint32_t> editDivePath;

	std::vector<sProp> GetProperties() {
		propNodeCount = static_cast<int>(nodes.size());
		propLinkCount = static_cast<int>(links.size());
		propNextId = static_cast<int>(nextId);
		return {{0, "Nodes", EPT_INT, &propNodeCount},
				{1, "Links", EPT_INT, &propLinkCount},
				{2, "Next id", EPT_INT, &propNextId}};
	}
};

} // namespace ecs
} // namespace rigkit
