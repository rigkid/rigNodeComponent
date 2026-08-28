#pragma once
#include <entt/entt.hpp>
#include "core/U_core.h"

class NodeDataHeroApp : public rigkit::IApp {
  public:
	NodeDataHeroApp() {
		window().width = 680;
		window().height = 440;
		window().title = "rigNodeComponent - nodedata";
		settings().appName = "nodedata";
	}
	void setup() override;
	void update(float dt) override;
	void draw() override;

  private:
	entt::entity m_graph = entt::null;
	float m_out = 0.f;
	float m_a = 1.f;
	float m_b = 2.f;
	float m_time = 0.f;
	int m_frames = 0;
	bool m_capture = false;
};
