#pragma once

namespace rigkit {
namespace project {
class ComponentSerializerRegistry;
}

namespace node {

/**
 * @brief Register `CNodeGraph` codecs on a document serializer registry.
 */
void registerProjectSerializers(project::ComponentSerializerRegistry& registry);

} // namespace node
} // namespace rigkit
