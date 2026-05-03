# insight-canon

**insight-canon** — tokenization and sequence: the foundational log-analysis pipeline.

**insight_canon** is a self-contained C++23 static library that provides the foundational
pipeline for structured log analysis:

| Layer | What it does |
|---|---|
| **core** | Shared types (`LogLevel`, `EventID`), `Result<T>`, logging façade (spdlog), ISO-8601 time utilities |
| **tokenization** | Format detection, Drain template clustering, arena allocator, `CanonicalEvent` output |
| **sequence** | Streaming flat history, sparse transition matrix, bounded n-gram counters, dominant-path reconstruction |

> A _canonical event_ is the normalized, format-agnostic representation of a log line.
> Tokenization produces it; sequence, insight-metalog, and insight-eidos all consume it.

All three layers are built as one library and consumed via a single CMake target: `insight::canon`.

---

## Pipeline

```text
Raw logs
  insight-canon   ->  CanonicalEvent  ->  event stream
  insight-metalog ->  bounded behavioral fingerprint
  insight-eidos   ->  detection reports + explain packets
```

---

## Requirements

| Tool | Minimum version |
|---|---|
| C++ compiler | GCC 13 or Clang 17 with C++23 support |
| CMake | 3.28 |
| Ninja | any recent |
| Conan | 2.x |

All library dependencies (spdlog, fmt, simdjson, GTest, nlohmann\_json) are resolved by Conan
from Conan Center Index — nothing needs to be installed manually.

---

## Quick start (Conan workflow)

### 1. Install dependencies and configure

```bash
# Install deps and generate CMake presets into build/
conan install . \
  --output-folder=build \
  --build=missing \
  --profile:host=linux-gcc13-release \
  --profile:build=linux-gcc13-release
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
  --repo coderoast-dev/insight-canon \
  --pattern 'insight_canon-*.tgz' \
  --dir /tmp/

conan cache restore /tmp/insight_canon-X.Y.Z.tgz
```

### Option B — build from source

```bash
git clone https://github.com/coderoast-dev/insight-canon.git
cd insight-canon
conan create . --profile:host=linux-gcc13-release --profile:build=linux-gcc13-release --build=missing
```

### CMake usage in your project

```cmake
find_package(insight_canon REQUIRED CONFIG)

target_link_libraries(my_target PRIVATE insight::canon)
```

In your `conanfile.py`:

```python
def requirements(self):
    self.requires("insight_canon/0.1.0")
```

---

## Project layout

```
insight-canon/
├── api/                    Public headers (install interface)
│   └── insight/
│       ├── core/           types.hpp
│       ├── utils/          result.hpp  logger.hpp  time_utils.hpp
│       ├── tokenization/   tokenizer_engine.hpp  canonical_event.hpp  …
│       └── sequence/       sequence_engine.hpp
├── src/                    Private implementation sources
│   └── insight/
│       ├── utils/
│       ├── tokenization/   strategies/  drain  arena  …
│       └── sequence/
├── test_package/           Conan consumer smoke test
├── tests/
│   ├── unit/               GTest unit tests for all layers
│   └── regression/         Loghub-dataset regression tests
├── scripts/
│   └── download_logs.sh    Download Loghub 2k + Zenodo datasets for regression
├── CMakeLists.txt          Single root CMake file
├── conanfile.py            Single Conan recipe
└── .github/workflows/      ci.yml  release-publish.yml  workflow-lint.yml
```

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
| `INSIGHT_CANON_ENABLE_NUMA` | `OFF` | Link libnuma for NUMA-aware arena allocation |

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
| `ci.yml` | PR touching `api/`, `src/`, `tests/`, `CMakeLists.txt`, or `conanfile.py` | `conan create` — builds the library, runs unit + regression tests, runs the test_package smoke test |
| `release-publish.yml` | Push of a `vX.Y.Z` tag (or manual dispatch) | Verifies recipe version matches tag, builds, exports a `conan cache save` tarball, attaches it to the GitHub Release |
| `workflow-lint.yml` | PR touching `.github/workflows/**` | Runs actionlint on all workflow files |

---

## License

Apache License 2.0 — see [LICENSE](LICENSE).
