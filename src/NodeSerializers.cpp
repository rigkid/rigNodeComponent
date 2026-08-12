#include "NodeSerializers.h"

#include "CNodeGraph.h"
#include "ComponentSerializerRegistry.h"
#include "NodeGraph.h"
#include "core/TypeJson.h"

#include <functional>
#include <memory>
#include <string>

namespace rigkit {
namespace node {
namespace {

using project::ComponentSerializer;
using project::ComponentSerializerRegistry;
using project::ordered_json;

template <typename T>
void addSerializer(ComponentSerializerRegistry& registry, const char* key, const char* schemaId,
				   bool (*serializeFn)(entt::registry&, entt::entity, ordered_json&),
				   bool (*deserializeFn)(entt::registry&, entt::entity, const ordered_json&)) {
	ComponentSerializer ser;
	ser.name = key;
	ser.schemaId = schemaId;
	ser.hasComponent = [](entt::registry& reg, entt::entity e) { return reg.all_of<T>(e); };
	ser.forEachEntity = [](entt::registry& reg, std::function<void(entt::entity)> fn) {
		for (auto entity : reg.view<T>()) {
			fn(entity);
		}
	};
	ser.serialize = serializeFn;
	ser.deserialize = deserializeFn;
	registry.registerSerializer(std::move(ser));
}

ordered_json serializeParam(const ecs::NodeParam& p) {
	ordered_json jp;
	jp["key"] = p.key;
	jp["type"] = p.type.empty() ? "float" : p.type;
	const std::string& t = p.type.empty() ? "float" : p.type;
	if (t == "float" || t == "double") {
		jp["f"] = p.f;
	} else if (t == "bool" || t == "int" || t == "uint" || t == "enum" || t == "entity") {
		jp["i"] = p.i;
	} else if (t == "string") {
		jp["s"] = p.s;
	} else if (t == "vec2" || t == "vec3" || t == "vec4" || t == "quat") {
		jp["v"] = vec4ToJson(p.v);
	} else {
		jp["f"] = p.f;
	}
	return jp;
}

ecs::NodeParam deserializeParam(const ordered_json& jp) {
	ecs::NodeParam p;
	p.key = jp.value("key", std::string{});
	p.type = jp.value("type", std::string{"float"});
	if (p.type.empty()) {
		p.type = "float";
	}
	if (jp.contains("f")) {
		p.f = jp.value("f", 0.f);
	}
	if (jp.contains("i")) {
		p.i = jp.value("i", 0);
	}
	if (jp.contains("s")) {
		p.s = jp.value("s", std::string{});
	}
	if (jp.contains("v")) {
		p.v = vec4FromJson(jp["v"]);
	}
	return p;
}

ordered_json serializeGraphData(const ecs::NodeGraphData& g);
ecs::NodeGraphData deserializeGraphData(const ordered_json& j);

ordered_json serializeNode(const ecs::GraphNode& n) {
	ordered_json jn;
	jn["id"] = n.id;
	jn["typeId"] = n.typeId;
	jn["title"] = n.title;
	jn["pos"] = vec2ToJson(n.pos);
	ordered_json pins = ordered_json::array();
	for (const auto& p : n.pins) {
		ordered_json jp;
		jp["id"] = p.id;
		jp["name"] = p.name;
		jp["kind"] = static_cast<int>(p.kind);
		jp["type"] = p.type;
		pins.push_back(std::move(jp));
	}
	jn["pins"] = std::move(pins);
	ordered_json params = ordered_json::array();
	for (const auto& p : n.params) {
		params.push_back(serializeParam(p));
	}
	jn["params"] = std::move(params);
	if (n.nested) {
		jn["nested"] = serializeGraphData(*n.nested);
	}
	if (!n.publishes.empty()) {
		ordered_json pubs = ordered_json::array();
		for (const auto& pub : n.publishes) {
			ordered_json jp;
			jp["pin"] = pub.pin;
			jp["innerNode"] = pub.innerNode;
			jp["innerPin"] = pub.innerPin;
			pubs.push_back(std::move(jp));
		}
		jn["publishes"] = std::move(pubs);
	}
	return jn;
}

ecs::GraphNode deserializeNode(const ordered_json& jn) {
	ecs::GraphNode n;
	n.id = jn.value("id", 0u);
	n.typeId = jn.value("typeId", std::string{});
	n.title = jn.value("title", std::string{});
	if (jn.contains("pos")) {
		n.pos = vec2FromJson(jn["pos"]);
	}
	if (jn.contains("pins") && jn["pins"].is_array()) {
		for (const auto& jp : jn["pins"]) {
			ecs::NodePin p;
			p.id = jp.value("id", 0u);
			p.name = jp.value("name", std::string{});
			p.kind = static_cast<ecs::NodePinKind>(jp.value("kind", 0));
			std::string raw;
			if (jp.contains("type")) {
				raw = jp.value("type", std::string{});
			} else {
				raw = jp.value("typeId", std::string{});
			}
			p.type = ::rig::node::normalizePinType(raw);
			n.pins.push_back(std::move(p));
		}
	}
	if (jn.contains("params") && jn["params"].is_array()) {
		for (const auto& jp : jn["params"]) {
			n.params.push_back(deserializeParam(jp));
		}
	}
	if (jn.contains("nested") && jn["nested"].is_object()) {
		n.nested = std::make_unique<ecs::NodeGraphData>(deserializeGraphData(jn["nested"]));
		if (n.typeId.empty()) {
			n.typeId = "group";
		}
	}
	if (jn.contains("publishes") && jn["publishes"].is_array()) {
		for (const auto& jp : jn["publishes"]) {
			ecs::NodePublish pub;
			pub.pin = jp.value("pin", 0u);
			pub.innerNode = jp.value("innerNode", 0u);
			pub.innerPin = jp.value("innerPin", 0u);
			n.publishes.push_back(pub);
		}
	}
	return n;
}

ordered_json serializeGraphData(const ecs::NodeGraphData& g) {
	ordered_json j;
	j["nextId"] = g.nextId;
	ordered_json nodes = ordered_json::array();
	for (const auto& n : g.nodes) {
		nodes.push_back(serializeNode(n));
	}
	j["nodes"] = std::move(nodes);
	ordered_json links = ordered_json::array();
	for (const auto& l : g.links) {
		ordered_json jl;
		jl["id"] = l.id;
		jl["fromNode"] = l.fromNode;
		jl["fromPin"] = l.fromPin;
		jl["toNode"] = l.toNode;
		jl["toPin"] = l.toPin;
		links.push_back(std::move(jl));
	}
	j["links"] = std::move(links);
	return j;
}

ecs::NodeGraphData deserializeGraphData(const ordered_json& j) {
	ecs::NodeGraphData g{};
	g.nextId = j.value("nextId", 1u);
	if (j.contains("nodes") && j["nodes"].is_array()) {
		for (const auto& jn : j["nodes"]) {
			g.nodes.push_back(deserializeNode(jn));
		}
	}
	if (j.contains("links") && j["links"].is_array()) {
		for (const auto& jl : j["links"]) {
			ecs::GraphLink l;
			l.id = jl.value("id", 0u);
			l.fromNode = jl.value("fromNode", 0u);
			l.fromPin = jl.value("fromPin", 0u);
			l.toNode = jl.value("toNode", 0u);
			l.toPin = jl.value("toPin", 0u);
			g.links.push_back(l);
		}
	}
	return g;
}

bool serializeNodeGraph(entt::registry& reg, entt::entity e, ordered_json& j) {
	if (!reg.all_of<ecs::CNodeGraph>(e)) {
		return false;
	}
	const auto& g = reg.get<ecs::CNodeGraph>(e);
	j = serializeGraphData(g);
	return true;
}

bool deserializeNodeGraph(entt::registry& reg, entt::entity e, const ordered_json& j) {
	ecs::CNodeGraph g{};
	static_cast<ecs::NodeGraphData&>(g) = deserializeGraphData(j);
	reg.emplace_or_replace<ecs::CNodeGraph>(e, std::move(g));
	return true;
}

} // namespace

void registerProjectSerializers(project::ComponentSerializerRegistry& registry) {
	addSerializer<ecs::CNodeGraph>(registry, "NodeGraph", "rig.node.graph", serializeNodeGraph,
								   deserializeNodeGraph);
}

} // namespace node
} // namespace rigkit
