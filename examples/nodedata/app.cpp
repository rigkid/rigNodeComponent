#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <string>

#include "core/RigKitEngine.h"
#include "core/pack/MPack.h"
#include "packs/rigComponent/src/rigComponent.h"
#include "packs/rigImGui/src/IWindow.h"
#include "packs/rigImGui/src/MWindow.h"
#include "packs/rigImGui/src/Mui.h"
#include "packs/rigImGui/src/UiDpi.h"
#include "packs/rigImGui/src/rigImGui.h"
#include "packs/rigNodeComponent/src/GraphEval.h"
#include "packs/rigNodeComponent/src/NodeGraph.h"
#include "packs/rigNodeComponent/src/rigNodeComponent.h"
#include "packs/rigSystems/src/rigSystems.h"
#include "rendering/U_gladGlfw.h"

namespace {

class NodeOutWindow : public rigkit::IWindow {
  public:
	NodeOutWindow(float* out, float* a, float* b)
		: IWindow("Demo Graph", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
									ImGuiWindowFlags_NoSavedSettings |
									ImGuiWindowFlags_NoScrollbar),
		  m_out(out), m_a(a), m_b(b) {}

	void render() override {
		if (!isVisible()) {
			return;
		}
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		const ImVec2 pad = ImGui::GetStyle().DisplayWindowPadding;
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + pad.x, vp->WorkPos.y + pad.y),
								ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(
			ImVec2(std::max(rigkit::uiPx(360.f), vp->WorkSize.x - pad.x * 2.f),
				   std::max(rigkit::uiPx(260.f), vp->WorkSize.y - pad.y * 2.f)),
			ImGuiCond_FirstUseEver);
		IWindow::render();
	}

  protected:
	void renderContents() override {
		const ImGuiStyle& style = ImGui::GetStyle();
		const float out = m_out ? *m_out : 0.f;
		const float a = m_a ? *m_a : 1.f;
		const float b = m_b ? *m_b : 2.f;

		ImGui::TextDisabled("seedDemoGraph  |  Value + Value -> Add -> Float Out");
		ImGui::Spacing();

		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float footerH = ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
		const float canvasW = std::max(rigkit::uiPx(200.f), avail.x);
		const float canvasH = std::max(rigkit::uiPx(200.f), avail.y - footerH);
		ImGui::InvisibleButton("graph_canvas", rigkit::uiHitSize(ImVec2(canvasW, canvasH)));
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 maxPt(origin.x + canvasW, origin.y + canvasH);
		const float rounding = style.FrameRounding;
		const float border =
			style.FrameBorderSize > 0.f ? style.FrameBorderSize : 1.f;
		dl->AddRectFilled(origin, maxPt, ImGui::GetColorU32(ImGuiCol_ChildBg), rounding);
		dl->AddRect(origin, maxPt, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, border);
		dl->PushClipRect(origin, maxPt, true);

		const float nx = std::clamp(canvasW * 0.24f, rigkit::uiPx(180.f), rigkit::uiPx(340.f));
		const float ny = std::clamp(canvasH * 0.24f, rigkit::uiPx(96.f), rigkit::uiPx(170.f));
		const float gapX = std::max(rigkit::uiPx(56.f), (canvasW - nx * 3.f) * 0.18f);
		const float col0 = origin.x + gapX;
		const float col1 = origin.x + canvasW * 0.5f - nx * 0.5f;
		const float col2 = origin.x + canvasW - gapX - nx;
		const float edgeY = std::max(rigkit::uiPx(40.f), canvasH * 0.10f);
		const float row0 = origin.y + edgeY;
		const float row1 = origin.y + canvasH - ny - edgeY;
		const float rowMid = origin.y + (canvasH - ny) * 0.5f;

		const ImU32 nodeBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
		const ImU32 nodeBorder = ImGui::GetColorU32(ImGuiCol_Border);
		const ImU32 titleCol = ImGui::GetColorU32(ImGuiCol_Text);
		const ImU32 bodyCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		const float textPadX = style.FramePadding.x;
		const float textPadY = style.FramePadding.y;

		auto nodeBox = [&](ImVec2 p, const char* title, const char* body, ImU32 accent) {
			const ImVec2 q(p.x + nx, p.y + ny);
			dl->AddRectFilled(p, q, nodeBg, rounding);
			dl->AddRectFilled(p, ImVec2(q.x, p.y + ny * 0.34f), accent, rounding);
			dl->AddRect(p, q, nodeBorder, rounding, 0, border);
			dl->AddText(ImVec2(p.x + textPadX, p.y + textPadY), titleCol, title);
			dl->AddText(ImVec2(p.x + textPadX, p.y + ny * 0.48f), bodyCol, body);
		};

		char bufA[32], bufB[32], bufOut[40];
		std::snprintf(bufA, sizeof(bufA), "value  %.3f", a);
		std::snprintf(bufB, sizeof(bufB), "value  %.3f", b);
		std::snprintf(bufOut, sizeof(bufOut), "out  %.3f", out);

		const ImVec2 aPos(col0, row0);
		const ImVec2 bPos(col0, row1);
		const ImVec2 addPos(col1, rowMid);
		const ImVec2 outPos(col2, rowMid);

		const ImU32 accentValue = ImGui::GetColorU32(ImGuiCol_SliderGrab);
		const ImU32 accentAdd = ImGui::GetColorU32(ImGuiCol_CheckMark);
		const ImU32 accentOut = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
		nodeBox(aPos, "Value A", bufA, accentValue);
		nodeBox(bPos, "Value B", bufB, accentValue);
		nodeBox(addPos, "Add", "a + b", accentAdd);
		nodeBox(outPos, "Float Out", bufOut, accentOut);

		const float pinR = rigkit::uiPx(4.5f);
		auto pin = [&](ImVec2 center) {
			dl->AddCircleFilled(center, pinR, ImGui::GetColorU32(ImGuiCol_Text));
			dl->AddCircle(center, pinR, ImGui::GetColorU32(ImGuiCol_Border), 0, border);
		};
		const ImVec2 aOut(aPos.x + nx, aPos.y + ny * 0.55f);
		const ImVec2 bOut(bPos.x + nx, bPos.y + ny * 0.55f);
		const ImVec2 addInA(addPos.x, addPos.y + ny * 0.35f);
		const ImVec2 addInB(addPos.x, addPos.y + ny * 0.70f);
		const ImVec2 addOut(addPos.x + nx, addPos.y + ny * 0.55f);
		const ImVec2 outIn(outPos.x, outPos.y + ny * 0.55f);
		pin(aOut);
		pin(bOut);
		pin(addInA);
		pin(addInB);
		pin(addOut);
		pin(outIn);

		const ImU32 wireCol = ImGui::GetColorU32(ImGuiCol_SliderGrabActive, 0.85f);
		const float wireThick = rigkit::uiPx(2.2f);
		auto wire = [&](ImVec2 from, ImVec2 to) {
			const float dx = std::max(rigkit::uiPx(28.f), (to.x - from.x) * 0.4f);
			dl->AddBezierCubic(from, ImVec2(from.x + dx, from.y), ImVec2(to.x - dx, to.y), to,
							   wireCol, wireThick);
		};
		wire(aOut, addInA);
		wire(bOut, addInB);
		wire(addOut, outIn);
		dl->PopClipRect();

		ImGui::SetCursorScreenPos(
			ImVec2(origin.x, origin.y + canvasH + style.ItemSpacing.y));
		ImGui::TextDisabled("Live eval each frame  |  edit seed values in code");
	}

	float* m_out = nullptr;
	float* m_a = nullptr;
	float* m_b = nullptr;
};
} // namespace

void NodeDataHeroApp::setup() {
	spdlog::info("nodedata - seedDemoGraph + evaluate");
	m_engine->setClearColor(0.09f, 0.10f, 0.12f, 1.f);
	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack(std::make_shared<rigkit::rigComponent>());
	packs->registerPack(std::make_shared<rigkit::rigSystems>());
	packs->registerPack(std::make_shared<rigkit::rigNodeComponent>());
	packs->registerPack(std::make_shared<rigkit::rigImGui>());
	packs->initAll();
	packs->setupAll();
	auto* ecs = m_engine->getECSManager();
	if (!ecs) {
		return;
	}
	m_graph = rig::node::makeGraph(*ecs, "nodedata-graph");
	auto& graph = ecs->getComponent<rigkit::ecs::CNodeGraph>(m_graph);
	rig::node::seedDemoGraph(graph);
	m_a = 1.f;
	m_b = 2.f;
	for (const auto& n : graph.nodes) {
		if (n.title == "Value A") {
			m_a = rig::node::getParamFloat(n, "value", 1.f);
		} else if (n.title == "Value B") {
			m_b = rig::node::getParamFloat(n, "value", 2.f);
		}
	}
	if (auto* ui = m_engine->getUiManager()) {
		if (auto* wm = ui->getWindowManager()) {
			wm->createWindow<NodeOutWindow>(&m_out, &m_a, &m_b);
			wm->showWindow("Demo Graph");
		}
	}
	if (const char* cap = std::getenv("RIGKIT_CAPTURE_PREVIEW")) {
		m_capture = (cap[0] != '\0' && cap[0] != '0');
		if (m_capture) {
			spdlog::info("nodedata - capture preview armed");
		}
	}
}

void NodeDataHeroApp::update(float dt) {
	m_time += dt;
	++m_frames;
	auto* ecs = m_engine->getECSManager();
	if (!ecs || m_graph == entt::null) {
		return;
	}
	rig::node::EvalContext ctx;
	ctx.time = m_time;
	ctx.dt = dt;
	const auto ev =
		rig::node::evaluateGraph(ecs->getComponent<rigkit::ecs::CNodeGraph>(m_graph), ctx);
	if (ev.ok && !ev.outputs.empty()) {
		m_out = ev.outputs[0];
	}

	if (!m_capture || !m_engine) {
		return;
	}
	// Place panel on frame 1, draw on frame 2+; export once ImGui has painted.
	if (m_frames == 40) {
		if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
			mui->requestExportPng();
		}
	} else if (m_frames == 45) {
		if (GLFWwindow* win = m_engine->getWindow()) {
			glfwSetWindowShouldClose(win, GLFW_TRUE);
		}
	}
}

void NodeDataHeroApp::draw() {}
