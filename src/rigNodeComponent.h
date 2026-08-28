#pragma once

#include "core/pack/IPack.h"
namespace rigkit {

/**
 * @brief Data pack - generic `CNodeGraph` PODs. No systems / no ImGui.
 */
class rigNodeComponent : public IPack {
  public:
	rigNodeComponent();
	bool init() override;
	void setup() override;
};

} // namespace rigkit
