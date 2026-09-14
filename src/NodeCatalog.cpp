#include "NodeCatalog.h"

namespace rig {
namespace node {
namespace {

using Kind = rigkit::ecs::NodePinKind;

const char* const kWaveLabels[] = {"Sine", "Triangle", "Saw", "Square"};
const char* const kCompareLabels[] = {"<", "<=", ">", ">=", "==", "!="};
const char* const kPortTypeLabels[] = {"Float", "Vec2", "Color"};

std::vector<CatalogEntry> makeCatalog() {
	std::vector<CatalogEntry> c;
	c.reserve(48);

	auto valueOut = []() -> std::vector<PinDesc> { return {{"out", Kind::Out, "float"}}; };
	auto unary = []() -> std::vector<PinDesc> {
		return {{"in", Kind::In, "float"}, {"out", Kind::Out, "float"}};
	};
	auto binary = []() -> std::vector<PinDesc> {
		return {{"a", Kind::In, "float"}, {"b", Kind::In, "float"}, {"out", Kind::Out, "float"}};
	};
	auto ternary = []() -> std::vector<PinDesc> {
		return {{"a", Kind::In, "float"},
				{"b", Kind::In, "float"},
				{"c", Kind::In, "float"},
				{"out", Kind::Out, "float"}};
	};

	// Values
	c.push_back({"float.value",
				 "Value",
				 "Values",
				 valueOut(),
				 {{"value", "Value", 0.f, -1000.f, 1000.f, 0.01f}}});
	c.push_back({"float.time", "Time", "Values", valueOut(), {}});
	c.push_back({"float.dt", "Delta", "Values", valueOut(), {}});

	// Maths
	c.push_back({"float.add", "Add", "Maths", binary(), {}});
	c.push_back({"float.sub", "Subtract", "Maths", binary(), {}});
	c.push_back({"float.mul", "Multiply", "Maths", binary(), {}});
	c.push_back({"float.div", "Divide", "Maths", binary(), {}});
	c.push_back({"float.mod", "Modulo", "Maths", binary(), {}});
	c.push_back({"float.pow", "Power", "Maths", binary(), {}});
	c.push_back({"float.min", "Min", "Maths", binary(), {}});
	c.push_back({"float.max", "Max", "Maths", binary(), {}});
	c.push_back({"float.abs", "Abs", "Maths", unary(), {}});
	c.push_back({"float.neg", "Negate", "Maths", unary(), {}});
	c.push_back({"float.floor", "Floor", "Maths", unary(), {}});
	c.push_back({"float.ceil", "Ceil", "Maths", unary(), {}});
	c.push_back({"float.fract", "Fract", "Maths", unary(), {}});
	c.push_back({"float.sin", "Sin", "Maths", unary(), {}});
	c.push_back({"float.cos", "Cos", "Maths", unary(), {}});
	c.push_back({"float.tan", "Tan", "Maths", unary(), {}});
	c.push_back({"float.clamp",
				 "Clamp",
				 "Maths",
				 ternary(),
				 {{"min", "Min", 0.f, -1000.f, 1000.f, 0.01f},
				  {"max", "Max", 1.f, -1000.f, 1000.f, 0.01f}}});
	c.push_back({"float.map",
				 "Map",
				 "Maths",
				 {{"in", Kind::In, "float"}, {"out", Kind::Out, "float"}},
				 {{"inMin", "In Min", 0.f, -1000.f, 1000.f, 0.01f},
				  {"inMax", "In Max", 1.f, -1000.f, 1000.f, 0.01f},
				  {"outMin", "Out Min", 0.f, -1000.f, 1000.f, 0.01f},
				  {"outMax", "Out Max", 1.f, -1000.f, 1000.f, 0.01f}}});
	c.push_back({"float.mix", "Mix", "Maths", ternary(), {}});
	c.push_back({"float.step", "Step", "Maths", binary(), {}});
	c.push_back({"float.smoothstep", "SmoothStep", "Maths", ternary(), {}});

	// Modulators
	c.push_back({"mod.lfo",
				 "LFO",
				 "Modulators",
				 valueOut(),
				 {{"freq", "Frequency", 1.f, 0.01f, 60.f, 0.01f},
				  {"amp", "Amplitude", 1.f, 0.f, 100.f, 0.01f},
				  {"offset", "Offset", 0.f, -100.f, 100.f, 0.01f},
				  {"phase", "Phase", 0.f, 0.f, 1.f, 0.01f},
				  {"wave", "Wave", 0.f, 0.f, 3.f, 1.f, 1, kWaveLabels, "enum"}}});
	c.push_back({"mod.noise",
				 "Noise",
				 "Modulators",
				 valueOut(),
				 {{"freq", "Frequency", 1.f, 0.01f, 60.f, 0.01f},
				  {"amp", "Amplitude", 1.f, 0.f, 100.f, 0.01f},
				  {"offset", "Offset", 0.f, -100.f, 100.f, 0.01f},
				  {"seed", "Seed", 0.f, 0.f, 10000.f, 1.f}}});
	c.push_back({"mod.random",
				 "Random",
				 "Modulators",
				 valueOut(),
				 {{"rate", "Rate Hz", 2.f, 0.01f, 60.f, 0.01f},
				  {"min", "Min", 0.f, -1000.f, 1000.f, 0.01f},
				  {"max", "Max", 1.f, -1000.f, 1000.f, 0.01f},
				  {"seed", "Seed", 1.f, 0.f, 10000.f, 1.f}}});
	c.push_back({"mod.ramp",
				 "Ramp",
				 "Modulators",
				 valueOut(),
				 {{"period", "Period", 2.f, 0.01f, 120.f, 0.01f},
				  {"from", "From", 0.f, -1000.f, 1000.f, 0.01f},
				  {"to", "To", 1.f, -1000.f, 1000.f, 0.01f}}});
	c.push_back({"mod.envelope",
				 "Envelope",
				 "Modulators",
				 {{"gate", Kind::In, "float"}, {"out", Kind::Out, "float"}},
				 {{"attack", "Attack", 0.05f, 0.001f, 10.f, 0.001f},
				  {"decay", "Decay", 0.1f, 0.001f, 10.f, 0.001f},
				  {"sustain", "Sustain", 0.7f, 0.f, 1.f, 0.01f},
				  {"release", "Release", 0.2f, 0.001f, 10.f, 0.001f}}});
	// Live audio input (see AudioBus.h - a capture pack publishes the values).
	c.push_back({"mod.audio",
				 "Audio",
				 "Modulators",
				 {{"level", Kind::Out, "float"},
				  {"bass", Kind::Out, "float"},
				  {"mid", Kind::Out, "float"},
				  {"treble", Kind::Out, "float"}},
				 {{"gain", "Gain", 1.f, 0.f, 20.f, 0.01f},
				  {"attack", "Attack", 0.02f, 0.001f, 1.f, 0.001f},
				  {"release", "Release", 0.25f, 0.001f, 2.f, 0.001f}}});
	// Live control voltage from a DC-coupled interface (see AudioBus.h CvBusValues).
	// One node = one channel, modular style. Raw bus value is -1..1 (±10V full
	// scale on an ES-8/9); out = raw * scale + offset, slewed over `slew` seconds.
	c.push_back({"mod.cv",
				 "CV In",
				 "Modulators",
				 valueOut(),
				 {{"channel", "Channel", 1.f, 1.f, 16.f, 1.f},
				  {"scale", "Scale", 1.f, -100.f, 100.f, 0.01f},
				  {"offset", "Offset", 0.f, -100.f, 100.f, 0.01f},
				  {"slew", "Slew s", 0.005f, 0.f, 5.f, 0.001f}}});

	// Logic
	c.push_back({"logic.compare",
				 "Compare",
				 "Logic",
				 binary(),
				 {{"op", "Op", 0.f, 0.f, 5.f, 1.f, 1, kCompareLabels, "enum"},
				  {"epsilon", "Epsilon", 0.0001f, 0.f, 1.f, 0.0001f}}});
	c.push_back(
		{"logic.gate",
		 "Gate",
		 "Logic",
		 {{"in", Kind::In, "float"}, {"gate", Kind::In, "float"}, {"out", Kind::Out, "float"}},
		 {}});
	c.push_back({"logic.switch",
				 "Switch",
				 "Logic",
				 {{"a", Kind::In, "float"},
				  {"b", Kind::In, "float"},
				  {"sel", Kind::In, "float"},
				  {"out", Kind::Out, "float"}},
				 {}});

	// Vectors
	c.push_back(
		{"vec2.value",
		 "Vec2",
		 "Vectors",
		 {{"out", Kind::Out, "vec2"}},
		 {{"x", "X", 0.f, -1000.f, 1000.f, 0.01f}, {"y", "Y", 0.f, -1000.f, 1000.f, 0.01f}}});
	c.push_back({"vec2.split",
				 "Split Vec2",
				 "Vectors",
				 {{"in", Kind::In, "vec2"}, {"x", Kind::Out, "float"}, {"y", Kind::Out, "float"}},
				 {}});
	c.push_back({"vec2.join",
				 "Join Vec2",
				 "Vectors",
				 {{"x", Kind::In, "float"}, {"y", Kind::In, "float"}, {"out", Kind::Out, "vec2"}},
				 {}});
	c.push_back({"vec2.add",
				 "Vec2 Add",
				 "Vectors",
				 {{"a", Kind::In, "vec2"}, {"b", Kind::In, "vec2"}, {"out", Kind::Out, "vec2"}},
				 {}});
	c.push_back({"vec2.sub",
				 "Vec2 Sub",
				 "Vectors",
				 {{"a", Kind::In, "vec2"}, {"b", Kind::In, "vec2"}, {"out", Kind::Out, "vec2"}},
				 {}});
	c.push_back({"vec2.mul",
				 "Vec2 Mul",
				 "Vectors",
				 {{"a", Kind::In, "vec2"}, {"b", Kind::In, "vec2"}, {"out", Kind::Out, "vec2"}},
				 {}});
	c.push_back({"vec2.scale",
				 "Vec2 Scale",
				 "Vectors",
				 {{"v", Kind::In, "vec2"}, {"s", Kind::In, "float"}, {"out", Kind::Out, "vec2"}},
				 {}});
	c.push_back({"vec2.length",
				 "Length",
				 "Vectors",
				 {{"in", Kind::In, "vec2"}, {"out", Kind::Out, "float"}},
				 {}});
	c.push_back({"vec2.normalize",
				 "Normalize",
				 "Vectors",
				 {{"in", Kind::In, "vec2"}, {"out", Kind::Out, "vec2"}},
				 {}});
	c.push_back({"vec2.dot",
				 "Dot",
				 "Vectors",
				 {{"a", Kind::In, "vec2"}, {"b", Kind::In, "vec2"}, {"out", Kind::Out, "float"}},
				 {}});

	// Color (catalog id keeps domain name; pin type is Rig `vec4`)
	c.push_back({"color.value",
				 "Color",
				 "Color",
				 {{"out", Kind::Out, "vec4"}},
				 {{"r", "R", 1.f, 0.f, 1.f, 0.01f},
				  {"g", "G", 1.f, 0.f, 1.f, 0.01f},
				  {"b", "B", 1.f, 0.f, 1.f, 0.01f},
				  {"a", "A", 1.f, 0.f, 1.f, 0.01f}}});
	c.push_back({"color.split",
				 "Split Color",
				 "Color",
				 {{"in", Kind::In, "vec4"},
				  {"r", Kind::Out, "float"},
				  {"g", Kind::Out, "float"},
				  {"b", Kind::Out, "float"},
				  {"a", Kind::Out, "float"}},
				 {}});
	c.push_back({"color.join",
				 "Join Color",
				 "Color",
				 {{"r", Kind::In, "float"},
				  {"g", Kind::In, "float"},
				  {"b", Kind::In, "float"},
				  {"a", Kind::In, "float"},
				  {"out", Kind::Out, "vec4"}},
				 {}});
	c.push_back({"color.mix",
				 "Mix Color",
				 "Color",
				 {{"a", Kind::In, "vec4"},
				  {"b", Kind::In, "vec4"},
				  {"t", Kind::In, "float"},
				  {"out", Kind::Out, "vec4"}},
				 {}});
	c.push_back(
		{"color.brightness",
		 "Brightness",
		 "Color",
		 {{"in", Kind::In, "vec4"}, {"gain", Kind::In, "float"}, {"out", Kind::Out, "vec4"}},
		 {}});

	// Output (app-readable lists - still useful; prefer Ref for scene bindings)
	c.push_back({"float.out", "Float Out", "Output", {{"in", Kind::In, "float"}}, {}});
	c.push_back({"vec2.out", "Vec2 Out", "Output", {{"in", Kind::In, "vec2"}}, {}});
	c.push_back({"color.out", "Color Out", "Output", {{"in", Kind::In, "vec4"}}, {}});

	// Ref - tiny sinks that write entity properties by name
	auto strParam = [](const char* key, const char* label) -> CatalogParam {
		CatalogParam p;
		p.key = key;
		p.label = label;
		p.type = "string";
		p.strDef = "";
		return p;
	};
	c.push_back({"ref.float",
				 "Float Ref",
				 "Ref",
				 {{"in", Kind::In, "float"}},
				 {strParam("entity", "Entity"), strParam("prop", "Property")}});
	c.push_back({"ref.vec2",
				 "Vec2 Ref",
				 "Ref",
				 {{"in", Kind::In, "vec2"}},
				 {strParam("entity", "Entity"), strParam("prop", "Property X"),
				  strParam("propY", "Property Y")}});
	c.push_back({"ref.color",
				 "Color Ref",
				 "Ref",
				 {{"in", Kind::In, "vec4"}},
				 {strParam("entity", "Entity"), strParam("prop", "Prop prefix")}});

	// Graph interface - inside a group, Export In / Export Out define the
	// group's outer pins (see syncGroupInterfaces / "Collapse to node").
	c.push_back({"graph.input",
				 "Export In",
				 "Graph",
				 {{"in", Kind::Out, "float"}},
				 {strParam("name", "Name"),
				  {"type", "Type", 0.f, 0.f, 2.f, 1.f, 1, kPortTypeLabels, "enum"},
				  {"value", "Default", 0.f, -1000.f, 1000.f, 0.01f}}});
	c.push_back({"graph.output",
				 "Export Out",
				 "Graph",
				 {{"out", Kind::In, "float"}},
				 {strParam("name", "Name"),
				  {"type", "Type", 0.f, 0.f, 2.f, 1.f, 1, kPortTypeLabels, "enum"}}});

	return c;
}

const std::vector<CatalogEntry>& catalogRef() {
	static const std::vector<CatalogEntry> kCatalog = makeCatalog();
	return kCatalog;
}

} // namespace

const std::vector<CatalogEntry>& builtinCatalog() {
	return catalogRef();
}

const CatalogEntry* findCatalogEntry(std::string_view typeId) {
	for (const auto& e : catalogRef()) {
		if (e.typeId == typeId) {
			return &e;
		}
	}
	return nullptr;
}

std::vector<const char*> catalogCategories() {
	std::vector<const char*> cats;
	for (const auto& e : catalogRef()) {
		bool seen = false;
		for (const char* c : cats) {
			if (std::string_view(c) == e.category) {
				seen = true;
				break;
			}
		}
		if (!seen) {
			cats.push_back(e.category);
		}
	}
	return cats;
}

uint32_t spawnCatalogNode(rigkit::ecs::NodeGraphData& graph, std::string_view typeId,
						  glm::vec2 pos) {
	const CatalogEntry* e = findCatalogEntry(typeId);
	if (!e) {
		return 0;
	}
	const uint32_t id = addNode(graph, e->typeId, e->title, pos, e->pins);
	if (auto* n = graph.findNode(id)) {
		for (const auto& p : e->params) {
			const char* ty = (p.type && p.type[0]) ? p.type : "float";
			if (std::string_view(ty) == "string") {
				setParamString(*n, p.key, p.strDef ? p.strDef : "", ty);
			} else if (std::string_view(ty) == "enum" || p.ui == 1) {
				setParamInt(*n, p.key, static_cast<int>(p.def), ty);
			} else {
				setParamFloat(*n, p.key, p.def, ty);
			}
		}
	}
	return id;
}

} // namespace node
} // namespace rig
