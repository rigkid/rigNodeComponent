#include "CompositeLibrary.h"

#include "NodeGraph.h"
#include "NodeSerializers.h"
#include "ProjectJson.h"
#include "core/util/AppPaths.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace rig {
namespace node {
namespace {

using rigkit::project::ordered_json;

void stripLibraryParam(rigkit::ecs::GraphNode& node) {
	auto& params = node.params;
	params.erase(std::remove_if(params.begin(), params.end(),
								 [](const rigkit::ecs::NodeParam& p) {
									 return p.key == kCompositeLibraryParam;
								 }),
				 params.end());
}

std::string slugFromTitle(std::string_view title) {
	std::string s;
	for (unsigned char c : title) {
		if (std::isalnum(c)) {
			s.push_back(static_cast<char>(std::tolower(c)));
		} else if (c == ' ' || c == '-' || c == '_') {
			if (!s.empty() && s.back() != '-') {
				s.push_back('-');
			}
		}
	}
	while (!s.empty() && s.back() == '-') {
		s.pop_back();
	}
	if (s.empty()) {
		s = "node";
	}
	return s;
}

/// Fresh ids from @p dest so a later ungroup cannot collide with the parent.
/// Returns the old-id to new-id map for this graph (nodes, pins, and links).
std::unordered_map<uint32_t, uint32_t> remapGraph(rigkit::ecs::NodeGraphData& dest,
												  rigkit::ecs::NodeGraphData& g) {
	std::unordered_map<uint32_t, uint32_t> map;
	auto take = [&](uint32_t old) -> uint32_t {
		if (old == 0) {
			return 0;
		}
		const auto it = map.find(old);
		if (it != map.end()) {
			return it->second;
		}
		const uint32_t n = dest.allocId();
		map.emplace(old, n);
		return n;
	};

	for (auto& n : g.nodes) {
		std::unordered_map<uint32_t, uint32_t> inner;
		if (n.nested) {
			inner = remapGraph(dest, *n.nested);
		}
		n.id = take(n.id);
		for (auto& p : n.pins) {
			p.id = take(p.id);
		}
		for (auto& pub : n.publishes) {
			if (const auto it = map.find(pub.pin); it != map.end()) {
				pub.pin = it->second;
			}
			if (const auto it = inner.find(pub.innerNode); it != inner.end()) {
				pub.innerNode = it->second;
			}
			if (const auto it = inner.find(pub.innerPin); it != inner.end()) {
				pub.innerPin = it->second;
			}
		}
	}
	for (auto& l : g.links) {
		l.id = take(l.id);
		if (const auto it = map.find(l.fromNode); it != map.end()) {
			l.fromNode = it->second;
		}
		if (const auto it = map.find(l.fromPin); it != map.end()) {
			l.fromPin = it->second;
		}
		if (const auto it = map.find(l.toNode); it != map.end()) {
			l.toNode = it->second;
		}
		if (const auto it = map.find(l.toPin); it != map.end()) {
			l.toPin = it->second;
		}
	}

	uint32_t next = 1;
	for (const auto& kv : map) {
		next = (std::max)(next, kv.second + 1);
	}
	g.nextId = next;
	return map;
}

bool readJson(const std::filesystem::path& path, ordered_json& out, std::string& error) {
	std::ifstream in{path, std::ios::binary};
	if (!in) {
		error = "could not read " + path.string();
		return false;
	}
	const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	try {
		out = ordered_json::parse(text);
	} catch (const std::exception& ex) {
		error = std::string("composite file is not json: ") + ex.what();
		return false;
	}
	if (!out.is_object()) {
		error = "composite file is not an object";
		return false;
	}
	return true;
}

bool formatOk(const ordered_json& j, std::string& error) {
	if (!j.contains("format") || !j["format"].is_number_integer()) {
		error = "composite file has no format";
		return false;
	}
	const int format = j["format"].get<int>();
	if (format != kCompositeFormat) {
		error = "composite format " + std::to_string(format) +
				" is not supported (this build reads 1)";
		return false;
	}
	return true;
}

bool endsWithNodeJson(const std::filesystem::path& path) {
	const std::string name = path.filename().string();
	constexpr std::string_view kSuffix = ".node.json";
	return name.size() >= kSuffix.size() &&
		   name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0;
}

} // namespace

std::string compositeLibraryDir() {
	return AppPaths::joinPath(AppPaths::getUserDataDir(), "user/nodes");
}

bool saveComposite(const rigkit::ecs::GraphNode& group, const std::filesystem::path& path,
				   std::string& error, std::string_view name, std::string_view category,
				   std::string_view description) {
	if (!group.isGroup()) {
		error = "only a group node can be saved to the library";
		return false;
	}
	std::error_code ec;
	if (!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), ec);
		if (ec) {
			error = "could not create " + path.parent_path().string();
			return false;
		}
	}

	rigkit::ecs::GraphNode copy = group;
	stripLibraryParam(copy);
	std::string storedName(name);
	if (storedName.empty()) {
		storedName = copy.title.empty() ? "Node" : copy.title;
	}
	std::string storedCategory(category);
	if (storedCategory.empty()) {
		storedCategory = "Library";
	}

	ordered_json j;
	j["format"] = kCompositeFormat;
	j["name"] = storedName;
	j["category"] = storedCategory;
	j["description"] = std::string(description);
	j["node"] = rigkit::node::serializeNodeJson(copy);

	std::ofstream out{path, std::ios::binary};
	if (!out) {
		error = "could not write " + path.string();
		return false;
	}
	out << j.dump(2) << '\n';
	if (!out) {
		error = "could not write " + path.string();
		return false;
	}
	return true;
}

bool loadComposite(const std::filesystem::path& path, CompositeFile& out, std::string& error) {
	ordered_json j;
	if (!readJson(path, j, error) || !formatOk(j, error)) {
		return false;
	}
	if (!j.contains("node") || !j["node"].is_object()) {
		error = "composite file has no node";
		return false;
	}
	out = {};
	out.node = rigkit::node::deserializeNodeJson(j["node"]);
	if (!out.node.isGroup()) {
		error = "composite node has no nested graph";
		return false;
	}
	out.name = j.value("name", std::string{});
	if (out.name.empty()) {
		out.name = out.node.title.empty() ? "Node" : out.node.title;
	}
	out.category = j.value("category", std::string{"Library"});
	if (out.category.empty()) {
		out.category = "Library";
	}
	out.description = j.value("description", std::string{});
	out.path = path.string();
	if (out.node.title.empty()) {
		out.node.title = out.name;
	}
	return true;
}

uint32_t spawnComposite(rigkit::ecs::NodeGraphData& graph, const rigkit::ecs::GraphNode& saved,
						glm::vec2 pos, std::string_view sourcePath, std::string& error) {
	if (!saved.isGroup()) {
		error = "composite node has no nested graph";
		return 0;
	}
	rigkit::ecs::GraphNode copy = saved;
	std::unordered_map<uint32_t, uint32_t> pinMap;
	auto takePin = [&](uint32_t old) -> uint32_t {
		if (old == 0) {
			return 0;
		}
		const auto it = pinMap.find(old);
		if (it != pinMap.end()) {
			return it->second;
		}
		const uint32_t n = graph.allocId();
		pinMap.emplace(old, n);
		return n;
	};

	copy.id = graph.allocId();
	for (auto& p : copy.pins) {
		p.id = takePin(p.id);
	}
	std::unordered_map<uint32_t, uint32_t> inner;
	if (copy.nested) {
		inner = remapGraph(graph, *copy.nested);
	}
	for (auto& pub : copy.publishes) {
		if (const auto it = pinMap.find(pub.pin); it != pinMap.end()) {
			pub.pin = it->second;
		}
		if (const auto it = inner.find(pub.innerNode); it != inner.end()) {
			pub.innerNode = it->second;
		}
		if (const auto it = inner.find(pub.innerPin); it != inner.end()) {
			pub.innerPin = it->second;
		}
	}
	copy.pos = pos;
	if (copy.typeId.empty()) {
		copy.typeId = "group";
	}
	if (!sourcePath.empty()) {
		setParamString(copy, kCompositeLibraryParam, sourcePath);
	}
	const uint32_t id = copy.id;
	graph.nodes.push_back(std::move(copy));
	error.clear();
	return id;
}

std::vector<CompositeEntry> listComposites(const std::filesystem::path& dir) {
	std::vector<CompositeEntry> found;
	std::error_code ec;
	std::filesystem::directory_iterator it(dir, ec);
	const std::filesystem::directory_iterator end;
	for (; !ec && it != end; it.increment(ec)) {
		if (ec || !it->is_regular_file(ec) || !endsWithNodeJson(it->path())) {
			continue;
		}
		ordered_json j;
		std::string error;
		if (!readJson(it->path(), j, error) || !formatOk(j, error)) {
			continue;
		}
		if (!j.contains("node") || !j["node"].is_object()) {
			continue;
		}
		CompositeEntry entry;
		entry.name = j.value("name", std::string{});
		if (entry.name.empty()) {
			entry.name = it->path().stem().stem().string();
		}
		if (entry.name.empty()) {
			entry.name = "Node";
		}
		entry.category = j.value("category", std::string{"Library"});
		if (entry.category.empty()) {
			entry.category = "Library";
		}
		entry.description = j.value("description", std::string{});
		entry.path = it->path().string();
		found.push_back(std::move(entry));
	}
	std::sort(found.begin(), found.end(), [](const CompositeEntry& a, const CompositeEntry& b) {
		if (a.name != b.name) {
			return a.name < b.name;
		}
		return a.path < b.path;
	});
	return found;
}

std::filesystem::path uniqueCompositePath(const std::filesystem::path& dir,
										  std::string_view title) {
	const std::string slug = slugFromTitle(title);
	std::filesystem::path candidate = dir / (slug + ".node.json");
	if (!std::filesystem::exists(candidate)) {
		return candidate;
	}
	for (int i = 2; i < 10000; ++i) {
		candidate = dir / (slug + "-" + std::to_string(i) + ".node.json");
		if (!std::filesystem::exists(candidate)) {
			return candidate;
		}
	}
	return dir / (slug + "-x.node.json");
}

} // namespace node
} // namespace rig
