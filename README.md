# rigNodeComponent

Generic **node graph** PODs - `CNodeGraph` graph/node/link records plus the
pure helpers that build them (`NodeGraph.h`: `makeGraph`, `addNode`, links,
catalog spawn shape). DATA ONLY: evaluation, the `SGraphEval` Update system,
the node catalog, and `.rig` codecs live in **rigNode**; editor chrome in
**rigNodeEditor**.

**Artist guide (editor, catalog, recipes):** [docs/nodes.md](https://github.com/rigkid/RigKit/blob/main/docs/nodes.md) in the RigKit host.

Pin types use Rig property datatypes: `float`, `vec2`, `vec4`, ... (colour pins are `vec4`; catalog ids like `color.value` keep domain names). Auto-coerce on links.

## Style

Format first-party `src/` and `examples/` with clang-format before commit (never `third_party/`).

```bash
./tools/format.sh
./tools/install-hooks.sh    # once per clone - pre-commit formats staged C/C++
```

Hooks only look for `clang-format` on PATH and warn, without formatting, when it is missing. Put it on PATH:

- **Windows:** install [LLVM](https://github.com/llvm/llvm-project/releases) (enable "Add LLVM to the system PATH"), or `winget install LLVM.LLVM`, then open a new shell. Confirm: `clang-format --version`.
- **macOS:** `brew install clang-format`
- **Linux:** `sudo apt install clang-format`

From a RigKit host checkout, `./tools/install-hooks.sh` wires every pack repo. Full policy: [RigKit packs Style](https://github.com/rigkid/RigKit/blob/main/packs/README.md#style).

[API/docs](https://rigkid.github.io/rigNodeComponent/)
