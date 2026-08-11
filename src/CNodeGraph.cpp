#include "CNodeGraph.h"

namespace rigkit {
namespace ecs {

GraphNode* NodeGraphData::findNode(uint32_t id) {
	for (auto& n : nodes) {
		if (n.id == id) {
			return &n;
		}
	}
	return nullptr;
}

const GraphNode* NodeGraphData::findNode(uint32_t id) const {
	for (const auto& n : nodes) {
		if (n.id == id) {
			return &n;
		}
	}
	return nullptr;
}

GraphLink* NodeGraphData::findLink(uint32_t id) {
	for (auto& l : links) {
		if (l.id == id) {
			return &l;
		}
	}
	return nullptr;
}

const NodePin* NodeGraphData::findPin(uint32_t nodeId, uint32_t pinId) const {
	const GraphNode* n = findNode(nodeId);
	if (!n) {
		return nullptr;
	}
	for (const auto& p : n->pins) {
		if (p.id == pinId) {
			return &p;
		}
	}
	return nullptr;
}

GraphNode::GraphNode() = default;
GraphNode::~GraphNode() = default;
GraphNode::GraphNode(GraphNode&&) noexcept = default;
GraphNode& GraphNode::operator=(GraphNode&&) noexcept = default;

GraphNode::GraphNode(const GraphNode& other)
	: id(other.id)
	, typeId(other.typeId)
	, title(other.title)
	, pos(other.pos)
	, pins(other.pins)
	, params(other.params)
	, publishes(other.publishes) {
	if (other.nested) {
		nested = std::make_unique<NodeGraphData>(*other.nested);
	}
}

GraphNode& GraphNode::operator=(const GraphNode& other) {
	if (this == &other) {
		return *this;
	}
	id = other.id;
	typeId = other.typeId;
	title = other.title;
	pos = other.pos;
	pins = other.pins;
	params = other.params;
	publishes = other.publishes;
	if (other.nested) {
		nested = std::make_unique<NodeGraphData>(*other.nested);
	} else {
		nested.reset();
	}
	return *this;
}

} // namespace ecs
} // namespace rigkit
