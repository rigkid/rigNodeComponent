#include "AudioBus.h"
#include "CNodeGraph.h"
#include "GraphEval.h"
#include "MusicQuantize.h"
#include "NodeCatalog.h"
#include "NodeGraph.h"

#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace {

bool nearEq(float a, float b, float eps = 1e-3f) {
	return std::fabs(a - b) <= eps;
}

} // namespace

int main() {
	rigkit::ecs::CNodeGraph graph;
	rig::node::seedDemoGraph(graph);
	auto ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.size() != 1 || !nearEq(ev.outputs[0], 3.f)) {
		std::printf("node_smoke FAIL demo eval\n");
		return 1;
	}

	graph = {};
	float y = 0.f;
	for (const auto& e : rig::node::builtinCatalog()) {
		if (!rig::node::spawnCatalogNode(graph, e.typeId, {0.f, y})) {
			std::printf("node_smoke FAIL spawn %s\n", e.typeId);
			return 1;
		}
		y += 40.f;
	}
	rig::node::EvalContext ctx;
	ctx.time = 0.25f;
	ctx.dt = 1.f / 60.f;
	std::unordered_map<uint32_t, float> scratch;
	ctx.scratch = &scratch;
	ev = rig::node::evaluateGraph(graph, ctx);
	if (!ev.ok) {
		std::printf("node_smoke FAIL catalog eval: %s\n", ev.error.c_str());
		return 1;
	}

	graph = {};
	const uint32_t lfo = rig::node::spawnCatalogNode(graph, "mod.lfo", {0.f, 0.f});
	const uint32_t out = rig::node::spawnCatalogNode(graph, "float.out", {200.f, 0.f});
	if (!rig::node::tryLinkByName(graph, lfo, "out", out, "in")) {
		std::printf("node_smoke FAIL LFO link\n");
		return 1;
	}
	if (graph.links.empty() || !rig::node::canLink(graph, lfo, graph.links[0].fromPin, out,
												   graph.links[0].toPin, graph.links[0].id)) {
		std::printf("node_smoke FAIL canLink ignore self\n");
		return 1;
	}
	const uint32_t val = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 80.f});
	const auto* valNode = graph.findNode(val);
	uint32_t valOut = 0;
	if (valNode) {
		for (const auto& p : valNode->pins) {
			if (p.kind == rigkit::ecs::NodePinKind::Out) {
				valOut = p.id;
				break;
			}
		}
	}
	const uint32_t lfoOut = graph.links[0].fromPin;
	const uint32_t outIn = graph.links[0].toPin;
	if (!val || !valOut || !rig::node::relink(graph, graph.links[0].id, val, valOut, out, outIn)) {
		std::printf("node_smoke FAIL relink\n");
		return 1;
	}
	if (!rig::node::relink(graph, graph.links[0].id, lfo, lfoOut, out, outIn)) {
		std::printf("node_smoke FAIL relink back\n");
		return 1;
	}
	ctx.time = 0.25f;
	ev = rig::node::evaluateGraph(graph, ctx);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 1.f)) {
		std::printf("node_smoke FAIL LFO\n");
		return 1;
	}

	// Vec2 length of (3,4) = 5
	graph = {};
	const uint32_t v = rig::node::spawnCatalogNode(graph, "vec2.value", {0.f, 0.f});
	const uint32_t len = rig::node::spawnCatalogNode(graph, "vec2.length", {200.f, 0.f});
	const uint32_t fout = rig::node::spawnCatalogNode(graph, "float.out", {400.f, 0.f});
	auto* nv = graph.findNode(v);
	if (!nv) {
		return 1;
	}
	rig::node::setParamFloat(*nv, "x", 3.f);
	rig::node::setParamFloat(*nv, "y", 4.f);
	if (!rig::node::tryLinkByName(graph, v, "out", len, "in") ||
		!rig::node::tryLinkByName(graph, len, "out", fout, "in")) {
		std::printf("node_smoke FAIL vec2 length link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 5.f)) {
		std::printf("node_smoke FAIL vec2 length=%f\n", ev.outputs.empty() ? -1.f : ev.outputs[0]);
		return 1;
	}

	// Vec2 Out + Color Out
	graph = {};
	const uint32_t vx = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 0.f});
	const uint32_t vy = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 60.f});
	const uint32_t join = rig::node::spawnCatalogNode(graph, "vec2.join", {200.f, 20.f});
	const uint32_t vout = rig::node::spawnCatalogNode(graph, "vec2.out", {400.f, 20.f});
	const uint32_t col = rig::node::spawnCatalogNode(graph, "color.value", {0.f, 140.f});
	const uint32_t cout = rig::node::spawnCatalogNode(graph, "color.out", {200.f, 140.f});
	auto* nx = graph.findNode(vx);
	auto* ny = graph.findNode(vy);
	auto* nc = graph.findNode(col);
	if (!nx || !ny || !nc) {
		return 1;
	}
	rig::node::setParamFloat(*nx, "value", 1.5f);
	rig::node::setParamFloat(*ny, "value", -2.5f);
	rig::node::setParamFloat(*nc, "r", 0.2f);
	rig::node::setParamFloat(*nc, "g", 0.4f);
	rig::node::setParamFloat(*nc, "b", 0.8f);
	rig::node::setParamFloat(*nc, "a", 1.f);
	if (!rig::node::tryLinkByName(graph, vx, "out", join, "x") ||
		!rig::node::tryLinkByName(graph, vy, "out", join, "y") ||
		!rig::node::tryLinkByName(graph, join, "out", vout, "in") ||
		!rig::node::tryLinkByName(graph, col, "out", cout, "in")) {
		std::printf("node_smoke FAIL typed-out link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.vec2Outputs.size() != 1 || ev.colorOutputs.size() != 1 ||
		!nearEq(ev.vec2Outputs[0].x, 1.5f) || !nearEq(ev.vec2Outputs[0].y, -2.5f) ||
		!nearEq(ev.colorOutputs[0].r, 0.2f) || !nearEq(ev.colorOutputs[0].g, 0.4f) ||
		!nearEq(ev.colorOutputs[0].b, 0.8f)) {
		std::printf("node_smoke FAIL typed outs\n");
		return 1;
	}

	std::printf("node_smoke ok catalog=%zu float/vec2/color outs\n",
				rig::node::builtinCatalog().size());

	// Nestable group: Value(2) + Value(5) to Add inside group to Float Out
	graph = {};
	const uint32_t ga = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 0.f});
	const uint32_t gb = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 80.f});
	const uint32_t gadd = rig::node::spawnCatalogNode(graph, "float.add", {200.f, 40.f});
	auto* nga = graph.findNode(ga);
	auto* ngb = graph.findNode(gb);
	if (!nga || !ngb) {
		return 1;
	}
	rig::node::setParamFloat(*nga, "value", 2.f);
	rig::node::setParamFloat(*ngb, "value", 5.f);
	if (!rig::node::tryLinkByName(graph, ga, "out", gadd, "a") ||
		!rig::node::tryLinkByName(graph, gb, "out", gadd, "b")) {
		std::printf("node_smoke FAIL group prep link\n");
		return 1;
	}
	const uint32_t gid = rig::node::createGroup(graph, {ga, gb, gadd}, {100.f, 40.f}, "Sum");
	const uint32_t gout = rig::node::spawnCatalogNode(graph, "float.out", {400.f, 40.f});
	auto* gnode = graph.findNode(gid);
	if (!gnode || !gnode->isGroup() || gnode->pins.empty()) {
		std::printf("node_smoke FAIL group create\n");
		return 1;
	}
	uint32_t groupOutPin = 0;
	for (const auto& p : gnode->pins) {
		if (p.kind == rigkit::ecs::NodePinKind::Out) {
			groupOutPin = p.id;
			break;
		}
	}
	if (groupOutPin == 0 ||
		!rig::node::tryLink(graph, gid, groupOutPin, gout, graph.findNode(gout)->pins[0].id)) {
		std::printf("node_smoke FAIL group out link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 7.f)) {
		std::printf("node_smoke FAIL group eval=%f ok=%d\n",
					ev.outputs.empty() ? -1.f : ev.outputs[0], ev.ok ? 1 : 0);
		return 1;
	}

	// Nested inject: parent drives published in to nested math to published out.
	// (createGroup smoke keeps Value nodes inside; this is the easy miss.)
	graph = {};
	const uint32_t gid2 = rig::node::createEmptyGroup(graph, {200.f, 40.f}, "Inject");
	auto* g2 = graph.findNode(gid2);
	if (!g2 || !g2->nested) {
		std::printf("node_smoke FAIL empty group\n");
		return 1;
	}
	const uint32_t innerAdd = rig::node::spawnCatalogNode(*g2->nested, "float.add", {200.f, 40.f});
	const uint32_t innerB = rig::node::spawnCatalogNode(*g2->nested, "float.value", {0.f, 80.f});
	auto* nInnerB = g2->nested->findNode(innerB);
	auto* nInnerAdd = g2->nested->findNode(innerAdd);
	if (!nInnerB || !nInnerAdd) {
		std::printf("node_smoke FAIL nested spawn\n");
		return 1;
	}
	rig::node::setParamFloat(*nInnerB, "value", 3.f);
	if (!rig::node::tryLinkByName(*g2->nested, innerB, "out", innerAdd, "b")) {
		std::printf("node_smoke FAIL nested link\n");
		return 1;
	}
	const uint32_t pubIn = rig::node::publishInnerPin(*g2, innerAdd, nInnerAdd->pins[0].id, "a");
	const uint32_t pubOut = rig::node::publishInnerPin(*g2, innerAdd, nInnerAdd->pins[2].id, "out");
	if (pubIn == 0 || pubOut == 0) {
		std::printf("node_smoke FAIL publish\n");
		return 1;
	}
	const uint32_t parentVal = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 40.f});
	const uint32_t parentOut = rig::node::spawnCatalogNode(graph, "float.out", {400.f, 40.f});
	auto* nParentVal = graph.findNode(parentVal);
	if (!nParentVal) {
		return 1;
	}
	rig::node::setParamFloat(*nParentVal, "value", 4.f);
	if (!rig::node::tryLinkByName(graph, parentVal, "out", gid2, "a") ||
		!rig::node::tryLinkByName(graph, gid2, "out", parentOut, "in")) {
		std::printf("node_smoke FAIL parent <-> group link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 7.f)) {
		std::printf("node_smoke FAIL nested inject eval=%f ok=%d err=%s\n",
					ev.outputs.empty() ? -1.f : ev.outputs[0], ev.ok ? 1 : 0, ev.error.c_str());
		return 1;
	}

	// Dive eval must inject parent seeds into the nested body.
	{
		rig::node::EvalContext ctx;
		auto dive = rig::node::evaluateAlongDive(graph, {gid2}, ctx);
		const auto* outPin = nInnerAdd->pins.size() >= 3 ? &nInnerAdd->pins[2] : nullptr;
		if (!dive.ok || !outPin) {
			std::printf("node_smoke FAIL dive eval ok=%d\n", dive.ok ? 1 : 0);
			return 1;
		}
		const auto it = dive.pinValue.find(rig::node::pinKey(innerAdd, outPin->id));
		if (it == dive.pinValue.end() || !nearEq(it->second.x, 7.f)) {
			std::printf("node_smoke FAIL dive inject val=%f\n",
						it == dive.pinValue.end() ? -1.f : it->second.x);
			return 1;
		}
	}

	if (!rig::node::ungroup(graph, gid2)) {
		std::printf("node_smoke FAIL ungroup\n");
		return 1;
	}
	if (graph.findNode(gid2) != nullptr) {
		std::printf("node_smoke FAIL group still present after ungroup\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 7.f)) {
		std::printf("node_smoke FAIL after ungroup eval=%f ok=%d err=%s\n",
					ev.outputs.empty() ? -1.f : ev.outputs[0], ev.ok ? 1 : 0, ev.error.c_str());
		return 1;
	}

	// Export In / Export Out define a collapsed group's interface.
	// in("a", default 2) -> add.a, value(3) -> add.b, add.out -> out("sum")
	graph = {};
	const uint32_t exIn = rig::node::spawnCatalogNode(graph, "graph.input", {0.f, 0.f});
	const uint32_t exVal = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 80.f});
	const uint32_t exAdd = rig::node::spawnCatalogNode(graph, "float.add", {200.f, 40.f});
	const uint32_t exOut = rig::node::spawnCatalogNode(graph, "graph.output", {400.f, 40.f});
	{
		auto* nIn = graph.findNode(exIn);
		auto* nVal = graph.findNode(exVal);
		auto* nOut = graph.findNode(exOut);
		if (!nIn || !nVal || !nOut) {
			std::printf("node_smoke FAIL export spawn\n");
			return 1;
		}
		rig::node::setParamString(*nIn, "name", "a");
		rig::node::setParamFloat(*nIn, "value", 2.f); // default when unconnected
		rig::node::setParamFloat(*nVal, "value", 3.f);
		rig::node::setParamString(*nOut, "name", "sum");
	}
	if (!rig::node::tryLinkByName(graph, exIn, "in", exAdd, "a") ||
		!rig::node::tryLinkByName(graph, exVal, "out", exAdd, "b") ||
		!rig::node::tryLinkByName(graph, exAdd, "out", exOut, "out")) {
		std::printf("node_smoke FAIL export link\n");
		return 1;
	}
	const uint32_t exGid =
		rig::node::createGroup(graph, {exIn, exVal, exAdd, exOut}, {0.f, 0.f}, "Comp");
	auto* exG = graph.findNode(exGid);
	if (!exG || !exG->isGroup()) {
		std::printf("node_smoke FAIL export collapse\n");
		return 1;
	}
	uint32_t pinA = 0;
	uint32_t pinSum = 0;
	for (const auto& p : exG->pins) {
		if (p.kind == rigkit::ecs::NodePinKind::In && p.name == "a") {
			pinA = p.id;
		} else if (p.kind == rigkit::ecs::NodePinKind::Out && p.name == "sum") {
			pinSum = p.id;
		}
	}
	if (pinA == 0 || pinSum == 0 || exG->pins.size() != 2) {
		std::printf("node_smoke FAIL export interface pins=%zu a=%u sum=%u\n", exG->pins.size(),
					pinA, pinSum);
		return 1;
	}
	const uint32_t exFOut = rig::node::spawnCatalogNode(graph, "float.out", {600.f, 40.f});
	if (!rig::node::tryLink(graph, exGid, pinSum, exFOut, graph.findNode(exFOut)->pins[0].id)) {
		std::printf("node_smoke FAIL export out link\n");
		return 1;
	}
	// Unconnected outer input -> Export In default applies: 2 + 3 = 5.
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 5.f)) {
		std::printf("node_smoke FAIL export default eval=%f ok=%d err=%s\n",
					ev.outputs.empty() ? -1.f : ev.outputs[0], ev.ok ? 1 : 0, ev.error.c_str());
		return 1;
	}
	// Drive the outer input: 4 + 3 = 7.
	const uint32_t exDrive = rig::node::spawnCatalogNode(graph, "float.value", {-200.f, 40.f});
	auto* nDrive = graph.findNode(exDrive);
	if (!nDrive) {
		return 1;
	}
	rig::node::setParamFloat(*nDrive, "value", 4.f);
	if (!rig::node::tryLink(graph, exDrive, nDrive->pins[0].id, exGid, pinA)) {
		std::printf("node_smoke FAIL export drive link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 7.f)) {
		std::printf("node_smoke FAIL export driven eval=%f ok=%d err=%s\n",
					ev.outputs.empty() ? -1.f : ev.outputs[0], ev.ok ? 1 : 0, ev.error.c_str());
		return 1;
	}

	std::printf("node_smoke ok groups + nested inject + dive + ungroup + export nodes\n");

	if (rig::node::quantizeMidi(62.f, 0, rig::node::MusicScale::Major) != 62 ||
		rig::node::quantizeMidi(61.f, 0, rig::node::MusicScale::Major) != 60 ||
		rig::node::quantizeMidi(61.f, 0, rig::node::MusicScale::Chromatic) != 61) {
		std::printf("node_smoke FAIL quantizeMidi\n");
		return 1;
	}

	graph = {};
	const uint32_t qv = rig::node::spawnCatalogNode(graph, "float.value", {0.f, 0.f});
	const uint32_t qn = rig::node::spawnCatalogNode(graph, "mod.quantize", {200.f, 0.f});
	const uint32_t qo = rig::node::spawnCatalogNode(graph, "float.out", {400.f, 0.f});
	auto* nQv = graph.findNode(qv);
	auto* nQn = graph.findNode(qn);
	if (!nQv || !nQn) {
		return 1;
	}
	rig::node::setParamFloat(*nQv, "value", 61.f);
	rig::node::setParamFloat(*nQn, "scale", 1.f); // major
	rig::node::setParamFloat(*nQn, "root", 0.f);
	rig::node::setParamFloat(*nQn, "input", 0.f);
	if (!rig::node::tryLinkByName(graph, qv, "out", qn, "in") ||
		!rig::node::tryLinkByName(graph, qn, "midi", qo, "in")) {
		std::printf("node_smoke FAIL quantize link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 60.f)) {
		std::printf("node_smoke FAIL quantize eval=%f\n",
					ev.outputs.empty() ? -1.f : ev.outputs[0]);
		return 1;
	}

	rig::node::AudioBusValues bus;
	bus.active = true;
	bus.centroid = 0.4f;
	bus.onset = 1.f;
	bus.note = 69.f;
	bus.pitchHz = 440.f;
	bus.chroma = 0.75f;
	bus.pitchConf = 0.9f;
	rig::node::setAudioBus(bus);
	graph = {};
	const uint32_t au = rig::node::spawnCatalogNode(graph, "mod.audio", {0.f, 0.f});
	const uint32_t ao = rig::node::spawnCatalogNode(graph, "float.out", {200.f, 0.f});
	if (!rig::node::tryLinkByName(graph, au, "onset", ao, "in")) {
		std::printf("node_smoke FAIL audio onset link\n");
		return 1;
	}
	ctx.time = 0.f;
	ctx.dt = 1.f / 60.f;
	scratch.clear();
	ctx.scratch = &scratch;
	ev = rig::node::evaluateGraph(graph, ctx);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 1.f)) {
		std::printf("node_smoke FAIL audio onset=%f\n", ev.outputs.empty() ? -1.f : ev.outputs[0]);
		return 1;
	}

	graph = {};
	const uint32_t note = rig::node::spawnCatalogNode(graph, "mod.note", {0.f, 0.f});
	const uint32_t nout = rig::node::spawnCatalogNode(graph, "float.out", {200.f, 0.f});
	auto* nNote = graph.findNode(note);
	if (!nNote) {
		return 1;
	}
	rig::node::setParamFloat(*nNote, "scale", 1.f);
	if (!rig::node::tryLinkByName(graph, note, "qmidi", nout, "in")) {
		std::printf("node_smoke FAIL note link\n");
		return 1;
	}
	ev = rig::node::evaluateGraph(graph);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 69.f)) {
		std::printf("node_smoke FAIL note qmidi=%f\n", ev.outputs.empty() ? -1.f : ev.outputs[0]);
		return 1;
	}

	rig::node::TempoBusValues tempo;
	tempo.active = true;
	tempo.bpm = 120.f;
	tempo.beatsPerBar = 4.f;
	tempo.beat = 0.98;
	rig::node::setTempoBus(tempo);
	graph = {};
	const uint32_t bt = rig::node::spawnCatalogNode(graph, "mod.beat", {0.f, 0.f});
	const uint32_t bout = rig::node::spawnCatalogNode(graph, "float.out", {200.f, 0.f});
	auto* nBt = graph.findNode(bt);
	if (!nBt) {
		return 1;
	}
	rig::node::setParamFloat(*nBt, "look", 0.04f);
	if (!rig::node::tryLinkByName(graph, bt, "ahead", bout, "in")) {
		std::printf("node_smoke FAIL beat ahead link\n");
		return 1;
	}
	scratch.clear();
	ctx.scratch = &scratch;
	ev = rig::node::evaluateGraph(graph, ctx);
	if (!ev.ok || ev.outputs.empty() || !nearEq(ev.outputs[0], 1.f)) {
		std::printf("node_smoke FAIL beat ahead=%f\n", ev.outputs.empty() ? -1.f : ev.outputs[0]);
		return 1;
	}

	std::printf("node_smoke ok audio / note / quantize / beat predict\n");
	return 0;
}
