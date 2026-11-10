# rigNodeComponent

Generic **node graph** PODs - `CNodeGraph` graph/node/link records plus the
pure helpers that build them (`NodeGraph.h`: `makeGraph`, `addNode`, links,
catalog spawn shape). DATA ONLY: evaluation, the `SGraphEval` Update system,
the node catalog, and `.rig` codecs live in **rigNode**; editor chrome in
**rigNodeEditor**.

**Artist guide (editor, catalog, recipes):** [docs/nodes.md](https://github.com/rigkid/RigKit/blob/main/docs/nodes.md) in the RigKit host.

Pin types use Rig property datatypes: `float`, `vec2`, `vec4`, ... (colour pins are `vec4`; catalog ids like `color.value` keep domain names). Auto-coerce on links.

[API/docs](https://rigkid.github.io/rigNodeComponent/)
