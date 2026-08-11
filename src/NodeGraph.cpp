#include "NodeGraph.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace rig {
namespace node {
namespace {

bool typesCompatible(std::string_view aIn, std::string_view bIn) {
	const std::string a = normalizePinType(aIn);
	const std::string b = normalizePinType(bIn);
	if (a.empty() || b.empty()) {
		return true;
	}
	if (a == b) {
		return true;
	}
	// Host coercion at eval: float ↔ vec2 ↔ vec4 (colour is vec4).
	const bool af = a == "float";
	const bool bf = b == "float";
	const bool av = a == "vec2";
	const bool bv = b == "vec2";
	const bool a4 = a == "vec4" || a == "quat" || a == "vec3";
	const bool b4 = b == "vec4" || b == "quat" || b == "vec3";
	if ((af || av || a4) && (bf || bv || b4)) {
		return true;
	}
	return false;
}

rigkit::ecs::NodeParam* findParam(rigkit::ecs::GraphNode& node, std::string_view key) {
	for (auto& p : node.params) {
		if (p.key == key) {
			return &p;
		}
	}
	return nullptr;
}

const rigkit::ecs::NodeParam* findParam(const rigkit::ecs::GraphNode& node, std::string_view key) {
	for (const auto& p : node.params) {
		if (p.key == key) {
			return &p;
		}
	}
	return nullptr;
}

} // namespace

std::string normalizePinType(std::string_view type) {
	if (type.empty() || type == "any") {
		return {};
	}
	if (type == "color") {
		return "vec4";
	}
	return std::string(type);
}

entt::entity makeGraph(rigkit::MEcs& ecs, std::string_view name) {
	const auto e = ecs.createEntity(std::string(name));
	ecs.addComponent<rigkit::ecs::CNodeGraph>(e, rigkit::ecs::CNodeGraph{});
	return e;
}

uint32_t addNode(rigkit::ecs::NodeGraphData& graph, std::string_view typeId, std::string_view title,
				 glm::vec2 pos, const std::vector<PinDesc>& pins) {
	rigkit::ecs::GraphNode node;
	node.id = graph.allocId();
	node.typeId = std::string(typeId);
	node.title = std::string(title);
	node.pos = pos;
	node.pins.reserve(pins.size());
	for (const auto& d : pins) {
		rigkit::ecs::NodePin pin;
		pin.id = graph.allocId();
		pin.name = d.name;
		pin.kind = d.kind;
		pin.type = normalizePinType(d.type);
		node.pins.push_back(std::move(pin));
	}
	const uint32_t id = node.id;
	graph.nodes.push_back(std::move(node));
	return id;
}

bool tryLink(rigkit::ecs::NodeGraphData& graph, uint32_t fromNode, uint32_t fromPin, uint32_t toNode,
			 uint32_t toPin) {
	if (fromNode == toNode) {
		return false;
	}
	const auto* from = graph.findPin(fromNode, fromPin);
	const auto* to = graph.findPin(toNode, toPin);
	if (!from || !to) {
		return false;
	}
	if (from->kind != rigkit::ecs::NodePinKind::Out || to->kind != rigkit::ecs::NodePinKind::In) {
		return false;
	}
	if (!typesCompatible(from->type, to->type)) {
		return false;
	}
	for (const auto& l : graph.links) {
		if (l.toNode == toNode && l.toPin == toPin) {
			return false;
		}
		if (l.fromNode == fromNode && l.fromPin == fromPin && l.toNode == toNode &&
			l.toPin == toPin) {
			return false;
		}
	}
	rigkit::ecs::GraphLink link;
	link.id = graph.allocId();
	link.fromNode = fromNode;
	link.fromPin = fromPin;
	link.toNode = toNode;
	link.toPin = toPin;
	graph.links.push_back(link);
	return true;
}

bool tryLinkByName(rigkit::ecs::NodeGraphData& graph, uint32_t fromNode, std::string_view fromPin,
				   uint32_t toNode, std::string_view toPin) {
	const auto* from = graph.findNode(fromNode);
	const auto* to = graph.findNode(toNode);
	if (!from || !to) {
		return false;
	}
	uint32_t fromPinId = 0;
	uint32_t toPinId = 0;
	for (const auto& p : from->pins) {
		if (p.kind == rigkit::ecs::NodePinKind::Out && p.name == fromPin) {
			fromPinId = p.id;
			break;
		}
	}
	for (const auto& p : to->pins) {
		if (p.kind == rigkit::ecs::NodePinKind::In && p.name == toPin) {
			toPinId = p.id;
			break;
		}
	}
	if (fromPinId == 0 || toPinId == 0) {
		return false;
	}
	return tryLink(graph, fromNode, fromPinId, toNode, toPinId);
}

void removeNode(rigkit::ecs::NodeGraphData& graph, uint32_t nodeId) {
	graph.links.erase(std::remove_if(graph.links.begin(), graph.links.end(),
									 [nodeId](const rigkit::ecs::GraphLink& l) {
										 return l.fromNode == nodeId || l.toNode == nodeId;
									 }),
					  graph.links.end());
	graph.nodes.erase(std::remove_if(graph.nodes.begin(), graph.nodes.end(),
									 [nodeId](const rigkit::ecs::GraphNode& n) {
										 return n.id == nodeId;
									 }),
					  graph.nodes.end());
}

void removeLink(rigkit::ecs::NodeGraphData& graph, uint32_t linkId) {
	graph.links.erase(std::remove_if(graph.links.begin(), graph.links.end(),
									 [linkId](const rigkit::ecs::GraphLink& l) {
										 return l.id == linkId;
									 }),
					  graph.links.end());
}

float getParamFloat(const rigkit::ecs::GraphNode& node, std::string_view key, float fallback) {
	if (const auto* p = findParam(node, key)) {
		if (p->type == "int" || p->type == "uint" || p->type == "bool" || p->type == "enum" ||
			p->type == "entity") {
			return static_cast<float>(p->i);
		}
		return p->f;
	}
	return fallback;
}

void setParamFloat(rigkit::ecs::GraphNode& node, std::string_view key, float value,
				   std::string_view type) {
	if (auto* p = findParam(node, key)) {
		p->type = std::string(type);
		p->f = value;
		return;
	}
	rigkit::ecs::NodeParam param;
	param.key = std::string(key);
	param.type = std::string(type);
	param.f = value;
	node.params.push_back(std::move(param));
}

int getParamInt(const rigkit::ecs::GraphNode& node, std::string_view key, int fallback) {
	if (const auto* p = findParam(node, key)) {
		if (p->type == "float" || p->type == "double") {
			return static_cast<int>(p->f);
		}
		return p->i;
	}
	return fallback;
}

void setParamInt(rigkit::ecs::GraphNode& node, std::string_view key, int value,
				 std::string_view type) {
	if (auto* p = findParam(node, key)) {
		p->type = std::string(type);
		p->i = value;
		p->f = static_cast<float>(value);
		return;
	}
	rigkit::ecs::NodeParam param;
	param.key = std::string(key);
	param.type = std::string(type);
	param.i = value;
	param.f = static_cast<float>(value);
	node.params.push_back(std::move(param));
}

glm::vec4 getParamVec4(const rigkit::ecs::GraphNode& node, std::string_view key,
					   const glm::vec4& fallback) {
	if (const auto* p = findParam(node, key)) {
		return p->v;
	}
	return fallback;
}

void setParamVec4(rigkit::ecs::GraphNode& node, std::string_view key, const glm::vec4& value,
				  std::string_view type) {
	if (auto* p = findParam(node, key)) {
		p->type = std::string(type);
		p->v = value;
		return;
	}
	rigkit::ecs::NodeParam param;
	param.key = std::string(key);
	param.type = std::string(type);
	param.v = value;
	node.params.push_back(std::move(param));
}

std::string getParamString(const rigkit::ecs::GraphNode& node, std::string_view key,
						   std::string_view fallback) {
	if (const auto* p = findParam(node, key)) {
		return p->s;
	}
	return std::string(fallback);
}

void setParamString(rigkit::ecs::GraphNode& node, std::string_view key, std::string_view value,
					std::string_view type) {
	if (auto* p = findParam(node, key)) {
		p->type = std::string(type);
		p->s = std::string(value);
		return;
	}
	rigkit::ecs::NodeParam param;
	param.key = std::string(key);
	param.type = std::string(type);
	param.s = std::string(value);
	node.params.push_back(std::move(param));
}

void seedDemoGraph(rigkit::ecs::CNodeGraph& graph) {
	graph = {};
	using Kind = rigkit::ecs::NodePinKind;
	const uint32_t a = addNode(graph, "float.value", "Value A", {40.f, 80.f},
							   {{"out", Kind::Out, "float"}});
	const uint32_t b = addNode(graph, "float.value", "Value B", {40.f, 220.f},
							   {{"out", Kind::Out, "float"}});
	const uint32_t add =
		addNode(graph, "float.add", "Add", {280.f, 140.f},
				{{"a", Kind::In, "float"}, {"b", Kind::In, "float"}, {"out", Kind::Out, "float"}});
	const uint32_t out =
		addNode(graph, "float.out", "Output", {520.f, 140.f}, {{"in", Kind::In, "float"}});

	if (auto* na = graph.findNode(a)) {
		setParamFloat(*na, "value", 1.f);
	}
	if (auto* nb = graph.findNode(b)) {
		setParamFloat(*nb, "value", 2.f);
	}

	const auto* na = graph.findNode(a);
	const auto* nb = graph.findNode(b);
	const auto* nAdd = graph.findNode(add);
	const auto* nOut = graph.findNode(out);
	if (!na || !nb || !nAdd || !nOut) {
		return;
	}
	tryLink(graph, a, na->pins[0].id, add, nAdd->pins[0].id);
	tryLink(graph, b, nb->pins[0].id, add, nAdd->pins[1].id);
	tryLink(graph, add, nAdd->pins[2].id, out, nOut->pins[0].id);
}

uint32_t createEmptyGroup(rigkit::ecs::NodeGraphData& graph, glm::vec2 pos, std::string_view title) {
	rigkit::ecs::GraphNode node;
	node.id = graph.allocId();
	node.typeId = "group";
	node.title = std::string(title);
	node.pos = pos;
	node.nested = std::make_unique<rigkit::ecs::NodeGraphData>();
	node.nested->nextId = 1;
	const uint32_t id = node.id;
	graph.nodes.push_back(std::move(node));
	return id;
}

uint32_t publishInnerPin(rigkit::ecs::GraphNode& group, uint32_t innerNode, uint32_t innerPin,
						 std::string_view outerName) {
	if (!group.nested) {
		return 0;
	}
	const auto* inner = group.nested->findPin(innerNode, innerPin);
	if (!inner) {
		return 0;
	}
	for (const auto& pub : group.publishes) {
		if (pub.innerNode == innerNode && pub.innerPin == innerPin) {
			return pub.pin; // already published
		}
	}
	rigkit::ecs::NodePin outer;
	// Outer pin ids only need uniqueness within group.pins.
	uint32_t nextPin = 1;
	for (const auto& p : group.pins) {
		nextPin = std::max(nextPin, p.id + 1);
	}
	outer.id = nextPin;
	outer.name = outerName.empty() ? inner->name : std::string(outerName);
	outer.kind = inner->kind;
	outer.type = inner->type;
	group.pins.push_back(outer);
	rigkit::ecs::NodePublish pub;
	pub.pin = outer.id;
	pub.innerNode = innerNode;
	pub.innerPin = innerPin;
	group.publishes.push_back(pub);
	return outer.id;
}

uint32_t createGroup(rigkit::ecs::NodeGraphData& graph, const std::vector<uint32_t>& nodeIds,
					 glm::vec2 pos, std::string_view title) {
	if (nodeIds.empty()) {
		return createEmptyGroup(graph, pos, title);
	}
	std::unordered_set<uint32_t> moveSet(nodeIds.begin(), nodeIds.end());
	glm::vec2 centroid{0.f, 0.f};
	int count = 0;
	for (uint32_t id : nodeIds) {
		if (auto* n = graph.findNode(id)) {
			centroid += n->pos;
			++count;
		}
	}
	if (count == 0) {
		return 0;
	}
	centroid /= static_cast<float>(count);

	const uint32_t groupId = createEmptyGroup(graph, pos.x != 0.f || pos.y != 0.f ? pos : centroid,
											  title);
	auto* group = graph.findNode(groupId);
	if (!group || !group->nested) {
		return 0;
	}

	// Move nodes into nested (preserve ids).
	std::vector<rigkit::ecs::GraphNode> moved;
	for (auto it = graph.nodes.begin(); it != graph.nodes.end();) {
		if (it->id == groupId) {
			++it;
			continue;
		}
		if (moveSet.count(it->id)) {
			it->pos -= centroid;
			it->pos += glm::vec2{80.f, 80.f};
			moved.push_back(std::move(*it));
			it = graph.nodes.erase(it);
		} else {
			++it;
		}
	}
	group = graph.findNode(groupId);
	if (!group || !group->nested) {
		return 0;
	}
	uint32_t maxId = group->nested->nextId;
	for (auto& n : moved) {
		maxId = std::max(maxId, n.id + 1);
		for (const auto& p : n.pins) {
			maxId = std::max(maxId, p.id + 1);
		}
		group->nested->nodes.push_back(std::move(n));
	}

	// Split links: internal stay in nested; crossing become publishes + outer links.
	std::vector<rigkit::ecs::GraphLink> remain;
	for (const auto& l : graph.links) {
		const bool fromIn = moveSet.count(l.fromNode) != 0;
		const bool toIn = moveSet.count(l.toNode) != 0;
		group = graph.findNode(groupId);
		if (!group) {
			break;
		}
		if (fromIn && toIn) {
			group->nested->links.push_back(l);
			maxId = std::max(maxId, l.id + 1);
		} else if (fromIn && !toIn) {
			const uint32_t outerPin = publishInnerPin(*group, l.fromNode, l.fromPin, {});
			if (outerPin != 0) {
				rigkit::ecs::GraphLink nl = l;
				nl.id = graph.allocId();
				nl.fromNode = groupId;
				nl.fromPin = outerPin;
				remain.push_back(nl);
			}
		} else if (!fromIn && toIn) {
			const uint32_t outerPin = publishInnerPin(*group, l.toNode, l.toPin, {});
			if (outerPin != 0) {
				rigkit::ecs::GraphLink nl = l;
				nl.id = graph.allocId();
				nl.toNode = groupId;
				nl.toPin = outerPin;
				remain.push_back(nl);
			}
		} else {
			remain.push_back(l);
		}
	}
	graph.links = std::move(remain);
	group = graph.findNode(groupId);
	if (group && group->nested) {
		group->nested->nextId = std::max(group->nested->nextId, maxId);
		// Publish interior pins that are not fully wired inside the nested graph
		// (free inputs / free outputs become the group interface).
		for (const auto& n : group->nested->nodes) {
			for (const auto& p : n.pins) {
				bool wired = false;
				for (const auto& l : group->nested->links) {
					if (p.kind == rigkit::ecs::NodePinKind::In && l.toNode == n.id &&
						l.toPin == p.id) {
						wired = true;
						break;
					}
					if (p.kind == rigkit::ecs::NodePinKind::Out && l.fromNode == n.id &&
						l.fromPin == p.id) {
						wired = true;
						break;
					}
				}
				if (!wired) {
					publishInnerPin(*group, n.id, p.id, {});
				}
			}
		}
	}
	return groupId;
}

bool ungroup(rigkit::ecs::NodeGraphData& graph, uint32_t groupId) {
	auto* group = graph.findNode(groupId);
	if (!group || !group->isGroup() || !group->nested) {
		return false;
	}

	const glm::vec2 base = group->pos;
	const auto publishes = group->publishes;
	rigkit::ecs::NodeGraphData nested = std::move(*group->nested);

	// Nested ids are scoped inside the group and may collide with the parent.
	// Lift every interior node to a fresh parent id before merging.
	std::unordered_map<uint32_t, uint32_t> idRemap;
	for (auto& n : nested.nodes) {
		const uint32_t old = n.id;
		n.id = graph.allocId();
		idRemap[old] = n.id;
		n.pos += base;
	}
	auto mapNested = [&](uint32_t id) -> uint32_t {
		const auto it = idRemap.find(id);
		return it == idRemap.end() ? id : it->second;
	};

	auto resolvePublish = [&](uint32_t outerPin, uint32_t& outNode, uint32_t& outPin) -> bool {
		for (const auto& pub : publishes) {
			if (pub.pin == outerPin) {
				outNode = mapNested(pub.innerNode);
				outPin = pub.innerPin;
				return idRemap.count(pub.innerNode) != 0;
			}
		}
		return false;
	};

	std::vector<rigkit::ecs::GraphLink> remain;
	remain.reserve(graph.links.size() + nested.links.size());
	for (const auto& l : graph.links) {
		if (l.fromNode == groupId && l.toNode == groupId) {
			continue;
		}
		if (l.fromNode == groupId) {
			uint32_t n = 0, p = 0;
			if (!resolvePublish(l.fromPin, n, p)) {
				continue;
			}
			rigkit::ecs::GraphLink nl = l;
			nl.fromNode = n;
			nl.fromPin = p;
			nl.id = graph.allocId();
			remain.push_back(nl);
			continue;
		}
		if (l.toNode == groupId) {
			uint32_t n = 0, p = 0;
			if (!resolvePublish(l.toPin, n, p)) {
				continue;
			}
			rigkit::ecs::GraphLink nl = l;
			nl.toNode = n;
			nl.toPin = p;
			nl.id = graph.allocId();
			remain.push_back(nl);
			continue;
		}
		remain.push_back(l);
	}

	for (auto& l : nested.links) {
		l.fromNode = mapNested(l.fromNode);
		l.toNode = mapNested(l.toNode);
		l.id = graph.allocId();
		remain.push_back(std::move(l));
	}

	graph.nodes.erase(std::remove_if(graph.nodes.begin(), graph.nodes.end(),
									 [groupId](const rigkit::ecs::GraphNode& n) {
										 return n.id == groupId;
									 }),
					  graph.nodes.end());
	for (auto& n : nested.nodes) {
		graph.nodes.push_back(std::move(n));
	}
	graph.links = std::move(remain);
	return true;
}

} // namespace node
} // namespace rig
