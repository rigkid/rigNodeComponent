# rigNodeComponent

![preview](examples/nodedata/img/preview.png)

Generic **node graph** PODs - `CNodeGraph` + catalog + eval + **`SGraphEval`**
(Update: evaluate every graph and `applyRefWrites`).

**Artist guide (editor, catalog, recipes):** [docs/nodes.md](https://github.com/rigkid/RigKit/blob/main/docs/nodes.md) in the RigKit host.

| Category | Nodes |
|----------|--------|
| Values | Value, Time, Delta |
| Maths | Add/Sub/Mul/Div/Mod/Pow, Min/Max, Abs/Negate, Floor/Ceil/Fract, Sin/Cos/Tan, Clamp, Map, Mix, Step, SmoothStep |
| Modulators | LFO, Noise, Random, Ramp, Envelope |
| Logic | Compare, Gate, Switch |
| Vectors | Vec2, Split/Join, Add/Sub/Mul/Scale, Length, Normalize, Dot |
| Color | Color, Split/Join, Mix, Brightness |
| Output | Float Out, Vec2 Out, Color Out |

Pin types use Rig property datatypes: `float`, `vec2`, `vec4`, ... (colour pins are `vec4`; catalog ids like `color.value` keep domain names). Auto-coerce on links. `.rig` codecs when **rigProject** is loaded.

```cpp
rig::node::spawnCatalogNode(graph, "mod.lfo", {40, 80});
auto ev = rig::node::evaluateGraph(graph, {time, dt, &scratch});
```

[API/docs](https://rigkid.github.io/rigNodeComponent/)
