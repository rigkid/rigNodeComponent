#pragma once

#include "CNodeGraph.h"
#include "ProjectJson.h"

namespace rigkit {
namespace project {
class ComponentSerializerRegistry;
}

namespace node {

/**
 * @brief Register `CNodeGraph` codecs on a document serializer registry.
 */
void registerProjectSerializers(project::ComponentSerializerRegistry& registry);

/** @brief One node, including `nested` and `publishes`, in the document codec. */
project::ordered_json serializeNodeJson(const ecs::GraphNode& node);

/** @brief Inverse of `serializeNodeJson`. */
ecs::GraphNode deserializeNodeJson(const project::ordered_json& json);

} // namespace node
} // namespace rigkit
