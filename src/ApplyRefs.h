#pragma once

#include "ecs/MEcs.h"
#include "GraphEval.h"

namespace rig {
namespace node {

/**
 * @brief Write `EvalResult::refWrites` into registered entity properties by name.
 * @details Looks up `entity` via `MEcs::findEntity`, then matches `prop` (and
 * optional `propY` / colour channel names) against `GetProperties()` metadata.
 */
void applyRefWrites(rigkit::MEcs& ecs, const EvalResult& result);

} // namespace node
} // namespace rig
