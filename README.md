# insight-canon

**insight-canon** — tokenization: the foundational log-analysis pipeline.

**insight_canon** is a self-contained C++23 static library that provides the foundational
pipeline for structured log analysis:

| Layer | What it does |
|---|---|
| **core** | Shared types (`LogLevel`, `EventID`), logging façade (spdlog), ISO-8601 time utilities |
| **tokenization** | Format detection, stateless per-line template masking, arena allocator, `CanonicalEvent` output |

> A _canonical event_ is the normalized, format-agnostic representation of a log line.
> Tokenization produces it; insight-metalog and insight-eidos consume it.

Both layers are built as one library and consumed via a single CMake target: `insight::canon`.

---

## Pipeline

```text
Raw logs
  insight-canon   ->  CanonicalEvent  ->  event stream
  insight-metalog ->  bounded behavioral fingerprint
  insight-eidos   ->  detection reports + explain packets
```

---

## Determinism

insight-canon is the **content-determinism** layer of the pipeline: the same input bytes always produce the same canonical events and the same numbers — on any compiler, architecture, or machine. That reproducibility is what makes everything built downstream benchmarkable and verifiable, run after run.

Two guarantees combine:

- **Deterministic tokenization** — a line's template is a pure function of its bytes: `same bytes ⇒ same tokens ⇒ same template id`, versioned by a `canonicalization_version`. `EventID`s are monotonic (never wall-clock or hash-seeded), `CanonicalEvent` string views are arena-stable, the template is a pure per-line function of its own masked tokens (no cross-line state), and no `unordered_map` iteration order ever reaches output.
- **Bit-identical math** (`det_math`) — every logarithm / entropy / divergence on the deterministic path is computed in integer fixed-point with **no libm** and accumulated in a 128-bit integer reducer (order-independent by construction). IEEE `+ − × ÷` are already cross-machine deterministic; removing libm transcendentals and float-sum ordering removes the only two divergence sources. Consuming code builds with `-ffp-contract=off`.

The result: identical logs yield an identical fingerprint *input* everywhere — the foundation the transport (`coderoast-ipc`) and the format (`insight-metalog`) build a fully reproducible pipeline on.

---

## Requirements

| Tool | Minimum version |
|---|---|
| C++ compiler | GCC 15 or Clang 21 with C++23 support |
| CMake | 3.28 |
| Ninja | any recent |
| Conan | 2.x |

All library dependencies (spdlog, fmt, simdjson, GTest, nlohmann\_json) are resolved by Conan
from Conan Center Index — nothing needs to be installed manually.

---

## Quick start

For local CodeRoast workspace iteration, use the parent `malf` helper from the
repo root:

```bash
malf build     # every package under the repo root, dependency-ordered (core -> semantic/* -> bench)
malf test      # same incremental build, then ctest per package
malf test core # just the core package
```

## Conan workflow

### 1. Install dependencies and configure

```bash
# Install deps and generate CMake presets into build/
conan install . \
  --output-folder=build \
  --build=missing \
  --profile:host=linux-gcc15-release \
  --profile:build=linux-gcc15-release
```

A `build/CMakePresets.json` will be generated. The repo root
`CMakeUserPresets.json` includes it automatically, so IDEs pick up the
presets without extra configuration.

### 2. Configure and build

```bash
cmake --preset conan-release
cmake --build --preset conan-release
```

### 3. Run tests

```bash
ctest --preset conan-release --output-on-failure
```

---

## Consuming as a Conan dependency

### Option A — from a GitHub Release tarball

Each tagged release attaches a `insight_canon-X.Y.Z.tgz` produced by
`conan cache save`. Restore it into your local cache:

```bash
# Download (requires gh CLI or manual download)
gh release download vX.Y.Z \
  --repo CodeRoasted/insight-canon \
  --pattern 'insight_canon-*.tgz' \
  --dir /tmp/

conan cache restore /tmp/insight_canon-X.Y.Z.tgz
```

### Option B — build from source

```bash
git clone https://github.com/CodeRoasted/insight-canon.git
cd insight-canon
conan create . --profile:host=linux-gcc15-release --profile:build=linux-gcc15-release --build=missing
```

### CMake usage in your project

```cmake
find_package(insight_canon REQUIRED CONFIG)

target_link_libraries(my_target PRIVATE insight::canon)
```

In your `conanfile.py`:

```python
def requirements(self):
  self.requires("insight_canon/<version>")  # pin to the current release
```

insight-canon is the upstream tokenization layer of the [MetaLog](https://github.com/CodeRoasted/metalog-spec) pipeline.

---

## Project layout

Multi-package Conan repo (ADR 0024): the root is the shelf; the tree reads like the
architecture — `core/` is the **language**, `semantic/*` are the **vocabularies**.

```
insight-canon/
├── core/                   insight_canon — the semantic-unaware core (Conan recipe + CMake)
│   ├── api/                PUBLIC (installed) module interface units
│   │   ├── canon.api.cppm        insight.canon.api — the contract (types, det_math,
│   │   │                         CanonicalEvent, MaskConfig, arena, utils, logging accessors)
│   │   ├── canon.spi.cppm        insight.canon.spi — the semantic-package provider contract
│   │   ├── canon.compose.cppm    insight.canon.compose — compose()/ComposedSemantics
│   │   ├── canon.cppm            insight.canon — the facade (Tokenizer; re-exports api + compose)
│   │   └── utils/log_macros.hpp  textual INSIGHT_LOG_* macro layer (installed header)
│   ├── src/                SEALED detail shards (build-only, never installed) + impl units
│   │   ├── scan/           insight.canon.detail.scan — fast_gates predicates + SSE2 sv_* scans
│   │   ├── strategy/       insight.canon.detail.strategy — IFormatStrategy + the format strategies
│   │   ├── mask/           insight.canon.detail.mask — the stateless per-line template masker
│   │   ├── parse/          insight.canon.detail.parse — FormatDetector + LogParser
│   │   ├── compose/        composition walkers (role/marker/location over composed rule rows)
│   │   ├── conformance/    insight.canon.conformance — the package conformance kit
│   │   └── ...             tokenizer/ arena/ identity/ utils/ impl units
│   ├── tests/              Per-domain mirror of src/ + the insight.canon.test aggregate module
│   ├── data/corpora/       Tracked corpus registry + smoke slices (ADR 0016)
│   └── test_package/       Conan consumer smoke test (zero-init, import insight.canon only)
├── semantic/               The vocabulary packages (statically composed, Apache-2.0)
│   ├── github/             insight_semantic_github — GitHub Actions / Azure dialect (rows + strategy)
│   ├── jenkins/            insight_semantic_jenkins — Jenkins dialect (rows + strategy)
│   └── test_frameworks/    insight_semantic_test_frameworks — test-file location families
├── bench/                  insight_canon_bench — the composed perf harness (SP-5 gate)
├── proof/                  Public determinism proof gate (composes core + both packages)
├── scripts/                det_public_proof.sh · sp1_semantic_unawareness_lint.sh · download_logs.sh
├── packages.yml            The package manifest (paths, versions, public/release flags)
└── .github/workflows/      ci.yml · lint.yml · golden.yaml · release.yaml
```

Module layering (the §11.9.11 pattern): `internal ◀ api ◀ detail.{scan ◀ strategy ◀ parse, mask} ◀
facade`. The facade interface never imports a detail shard; `tokenizer_engine.cpp` (a facade impl
unit) imports `detail.{strategy,mask,parse}` to assemble the pipeline — consumers just
`import insight.canon;`.

---

## Building and running tests locally (without Conan)

If you already have the dependencies available as CMake packages, you can
configure directly:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DINSIGHT_CANON_BUILD_TESTS=ON \
  -DCMAKE_CXX_STANDARD=23

cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

---

## CMake options

| Option | Default | Description |
|---|---|---|
| `INSIGHT_CANON_BUILD_TESTS` | `ON` when top-level | Build unit and regression tests |
| `INSIGHT_CANON_ENABLE_NUMA` | `OFF` | Link libnuma (LGPL-2.1) for NUMA-aware arena allocation. Opt-in via the conan `with_numa` option; off keeps the package's dep tree all-permissive. NUMA-off is bit-identical to NUMA-on and a no-op on single-socket hosts. |

---

## Code style

The project uses clang-format (`.clang-format`) and clang-tidy (`.clang-tidy`) with
settings checked in at the repo root.

```bash
# Format all sources in-place
clang-format -i $(find api src tests test_package -name '*.cpp' -o -name '*.hpp')

# Lint (requires compile_commands.json in build/)
clang-tidy -p build $(find src -name '*.cpp')
```

---

## Regression tests (Loghub datasets)

The regression suite in `tests/regression/` tokenizes 16 real-world log files from the
[Loghub 2k](https://github.com/logpai/loghub) benchmark and asserts minimum per-dataset
parse-success rates. The test binary auto-skips if the data directory is absent, so a
normal `conan create` or `ctest` run is always clean without them.

### 1. Download the datasets

```bash
bash scripts/download_logs.sh
```

This populates:
```
data/logs/loghub/   ← 16 × *_2k.log files from the logpai/loghub GitHub repo
data/logs/zenodo/   ← extended archive from Zenodo record 18522101
```

Requires `curl` and either `unzip` or `bsdtar`.

### 2. Run the regression tests

The test binary looks for `data/logs/loghub/` relative to its working directory.
When running via CTest the working directory is the build folder, so symlink or
copy the data directory there, or set the working directory explicitly:

```bash
# Conan workflow — run from the build output directory
cd build/<preset-dir>
ln -s ../../data data       # or: cp -r ../../data data
ctest --output-on-failure -R regression
```

```bash
# CMake direct workflow
cmake --build build -j$(nproc)
cd build
ln -s ../data data
ctest --output-on-failure -R regression
```

To override the minimum success threshold for all datasets:

```bash
INSIGHT_TOKENIZER_REGRESSION_MIN_SUCCESS_RATE=0.90 ctest --output-on-failure -R regression
```

---

## CI

| Workflow | Trigger | What it does |
|---|---|---|
| `ci.yml` | PRs to main | Dependency-ordered `conan create` of all five packages (core → semantic/* → bench) + tests + the test_package smoke test |
| `release.yaml` | Push of a `vX.Y.Z` tag (or manual dispatch) | Lint → CI → 5-leg determinism golden gate → measures the composed benchmark → verifies recipe versions, exports `conan cache save` tarballs, attaches them + the golden to the GitHub Release |
| `workflow-lint.yml` | PR touching `.github/workflows/**` | Runs actionlint on all workflow files |

---

## Technical Docs

Pipeline reference for tokenization lives in [technical_docs/](technical_docs/README.md).

---

## License

Apache-2.0 — see [LICENSE](LICENSE).
