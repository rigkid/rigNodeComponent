#pragma once

#include "ecs/MEcs.h"

namespace rigkit {
namespace ecs {

/**
 * @brief Evaluate every `CNodeGraph` and apply `ref.*` writes each Update.
 * @details Fulfillment for node Ref sinks — hosts need not call `evaluateGraph` /
 * `applyRefWrites` themselves. Editor dive preview stays in `rigNodeEditor`
 * (`evaluateAlongDive` without apply).
 */
void SGraphEval(MEcs& ecs, float dt);

} // namespace ecs
} // namespace rigkit
