#pragma once

#include "CNodeGraph.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rig {
namespace node {

/// File envelope this build writes and reads. Other values are rejected.
inline constexpr int kCompositeFormat = 1;

/**
 * @brief String param on a spawned group that remembers the library file.
 * @details Scene save keeps it so Update still works after a reload. The library
 * file itself does not store this param.
 */
inline constexpr const char* kCompositeLibraryParam = "library";

/**
 * @brief One saved group under a library directory.
 */
struct CompositeEntry {
	std::string name;
	std::string category;
	std::string description;
	std::string path;
};

/**
 * @brief A library file loaded into a group node.
 */
struct CompositeFile {
	std::string name;
	std::string category;
	std::string description;
	std::string path;
	rigkit::ecs::GraphNode node;
};

/**
 * @brief `<userData>/user/nodes`, where the editor lists `*.node.json`.
 */
std::string compositeLibraryDir();

/**
 * @brief Write @p group as a format-1 library file.
 * @details @p name defaults to the node title. The stored node drops the `library` param.
 * @return False writes a reason into @p error.
 */
bool saveComposite(const rigkit::ecs::GraphNode& group, const std::filesystem::path& path,
				   std::string& error, std::string_view name = {},
				   std::string_view category = "Library", std::string_view description = {});

/**
 * @brief Read a library file. Rejects a missing or unsupported `format`.
 */
bool loadComposite(const std::filesystem::path& path, CompositeFile& out, std::string& error);

/**
 * @brief Deep-copy @p saved into @p graph at @p pos.
 * @details The group id and every nested node, pin, and link id are taken from
 * `graph.allocId()`, so the copy does not collide with the parent (same lift `ungroup` does).
 * When @p sourcePath is set, the copy's `library` param remembers that file.
 * @return New group id, or 0. @p error is set on failure.
 */
uint32_t spawnComposite(rigkit::ecs::NodeGraphData& graph, const rigkit::ecs::GraphNode& saved,
						glm::vec2 pos, std::string_view sourcePath, std::string& error);

/**
 * @brief List `*.node.json` in @p dir. Skips files this build cannot read.
 */
std::vector<CompositeEntry> listComposites(const std::filesystem::path& dir);

/**
 * @brief `dir/<slug>.node.json`, or `dir/<slug>-2.node.json` when that name is taken.
 */
std::filesystem::path uniqueCompositePath(const std::filesystem::path& dir,
										  std::string_view title);

} // namespace node
} // namespace rig
