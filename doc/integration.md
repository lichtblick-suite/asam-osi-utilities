<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Integrating OSIUtilities into Your Project

This guide covers how to use the OSIUtilities C++ library in a downstream CMake
project. Three integration methods are available — choose based on your
dependency management strategy and caching requirements.

## Quick Start

After installation (any method), usage in CMake is always the same:

```cmake
find_package(OSIUtilities REQUIRED)
target_link_libraries(my_target PRIVATE OSIUtilities::OSIUtilities)
```

This gives you:

- The OSIUtilities library (MCAP/binary/TXTH trace file readers and writers)
- OSI C++ bindings (osi3 protobuf headers and libraries, transitively)
- Protobuf (transitively, via OSI)
- MCAP headers (transitively)
- LZ4 and ZSTD compression (linked internally, headers not exposed)

## Integration Methods Overview

| Method | Dependency control | Caching | Complexity | Best for |
| --- | --- | --- | --- | --- |
| **A. Two-phase build** | Baseline (vcpkg) or custom | ✅ Excellent | Medium | CI pipelines, large projects |
| **B. add_subdirectory** | Full (your project controls) | Via ccache | Low | Embedded/submodule workflows |
| **C. System install** | Full (system packages) | OS package cache | Low | Linux distro packaging |

---

## Method A: Two-Phase Build with `find_package()` (Recommended)

Build and install OSIUtilities and its dependencies in a separate phase, then
consume via `find_package()`. This gives **excellent CI caching** — the
dependency install directory can be cached and only rebuilt when the submodule
is bumped.

### Step 1: Add as a submodule

```bash
git submodule add https://github.com/lichtblick-suite/asam-osi-utilities.git \
    externals/asam-osi-utilities
git submodule update --init --recursive
```

### Step 2: Phase A — Build and install dependencies

```bash
# Configure (uses vcpkg for protobuf, lz4, zstd)
cmake -S externals/asam-osi-utilities \
      -B build-deps \
      --preset vcpkg \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTING=OFF

# Build
cmake --build build-deps --config Release --parallel

# Install to a local prefix
cmake --install build-deps --config Release --prefix deps/
```

This produces a self-contained `deps/` directory with all headers, libraries,
and CMake config files.

### Step 3: Phase B — Build your project

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(my_project)

find_package(OSIUtilities REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE OSIUtilities::OSIUtilities)
```

Configure with the install prefix and vcpkg dependencies:

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH=deps/ \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_INSTALLED_DIR=build-deps/vcpkg_installed \
      -DVCPKG_MANIFEST_MODE=OFF

cmake --build build --config Release
```

**Key flags explained:**

| Flag | Purpose |
| --- | --- |
| `CMAKE_PREFIX_PATH=deps/` | Tells CMake where to find OSIUtilities and OSI configs |
| `CMAKE_TOOLCHAIN_FILE` | vcpkg toolchain ensures protobuf is found in CONFIG mode (required for abseil transitive deps) |
| `VCPKG_INSTALLED_DIR` | Points to Phase A's vcpkg-installed packages (protobuf, lz4, zstd) |
| `VCPKG_MANIFEST_MODE=OFF` | Prevents vcpkg from trying to install packages again — uses existing ones |

### CI Caching

Cache the `deps/` directory and `build-deps/vcpkg_installed/` keyed on the
submodule commit hash:

```yaml
# GitHub Actions example
- uses: actions/cache@v4
  with:
    path: |
      deps/
      build-deps/vcpkg_installed/
    key: osi-utils-${{ hashFiles('externals/asam-osi-utilities/**') }}
```

Phase A only re-runs when the submodule is bumped. Phase B (your project) uses
the cached artifacts and builds in seconds.

---

## Method B: `add_subdirectory()` (Simplest)

Embed asam-osi-utilities directly in your CMake build tree. This is the
simplest approach and gives your project full control over all dependencies.

### Step 1: Add as a submodule

```bash
git submodule add https://github.com/lichtblick-suite/asam-osi-utilities.git \
    externals/asam-osi-utilities
git submodule update --init --recursive
```

### Step 2: Include in your CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

# Provide protobuf, lz4, zstd before adding the subdirectory.
# Option 1: vcpkg toolchain (recommended)
# Option 2: find_package(Protobuf), find_package(lz4), find_package(zstd)
# Option 3: System packages (Linux)

# Disable parts you don't need
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory(externals/asam-osi-utilities)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE OSIUtilities::OSIUtilities)
```

### Dependency resolution

With `add_subdirectory()`, **your project** is responsible for making protobuf,
lz4, and zstd available. Common approaches:

**vcpkg (cross-platform):** Add a `vcpkg.json` to your project:

```json
{
  "dependencies": ["protobuf", "lz4", "zstd"]
}
```

Then configure with `CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`.

**System packages (Linux):**

```bash
# Debian/Ubuntu
sudo apt install libprotobuf-dev protobuf-compiler liblz4-dev libzstd-dev

# Fedora/RHEL
sudo dnf install protobuf-devel lz4-devel libzstd-devel
```

Then configure with the `base` preset or plain `cmake`.

### Trade-offs

- ✅ Simplest setup — no install step, targets available directly
- ✅ Full control over dependency versions (your CMake environment provides them)
- ❌ No caching benefit — asam-osi-utilities recompiles with every clean build
- ❌ OSI protobuf code generation runs every build (can be slow)

---

## Method C: System Install + `find_package()`

Install asam-osi-utilities system-wide or to a custom prefix, then consume via
`find_package()` in any project. Useful for Linux distro packaging or shared
development environments.

### Step 1: Build and install

```bash
git clone --recurse-submodules \
    https://github.com/lichtblick-suite/asam-osi-utilities.git
cd asam-osi-utilities

# Using system packages (no vcpkg)
cmake --preset base \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TESTING=OFF
cmake --build build --config Release --parallel
sudo cmake --install build --config Release
```

This installs to `/usr/local/` by default. Override with `--prefix`:

```bash
cmake --install build --config Release --prefix /opt/osi-utilities
```

### Step 2: Use in your project

```cmake
find_package(OSIUtilities REQUIRED)
target_link_libraries(my_app PRIVATE OSIUtilities::OSIUtilities)
```

If installed to a non-standard prefix:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/osi-utilities
```

---

## Customizing Dependencies

### Using a different protobuf version

Protobuf is a **public dependency** — `google::protobuf::Message` appears in
the OSIUtilities API. The consumer and OSIUtilities **must use the same
protobuf version** (ABI-compatible) to avoid ODR violations.

**With two-phase build (Method A):** The protobuf version is determined by the
vcpkg baseline in `vcpkg-configuration.json`. To override:

```bash
# Option 1: Use the base preset with your own protobuf
cmake -S externals/asam-osi-utilities -B build-deps \
      --preset base \
      -DProtobuf_DIR=/path/to/your/protobuf/lib/cmake/protobuf

# Option 2: Override vcpkg baseline
# Create your own vcpkg-configuration.json with a different baseline hash
# and pass it via VCPKG_MANIFEST_DIR
cmake -S externals/asam-osi-utilities -B build-deps \
      --preset vcpkg \
      -DVCPKG_MANIFEST_DIR=/path/to/your/vcpkg-manifest/
```

**With add_subdirectory (Method B):** Your project provides protobuf. If you
call `find_package(Protobuf)` before `add_subdirectory(asam-osi-utilities)`,
your protobuf is used automatically (CMake's `find_package` is a no-op when
the package is already found).

### Controlling compression libraries (lz4, zstd)

LZ4 and ZSTD are **private dependencies** — they are not exposed in any public
header. Consumers do not need their headers, but the libraries must be
available at link time (CMake handles this automatically for both static and
shared builds).

To use custom lz4/zstd installations:

```bash
cmake -S externals/asam-osi-utilities -B build-deps \
      --preset base \
      -DCMAKE_PREFIX_PATH=/path/to/custom/lz4:/path/to/custom/zstd
```

### Static vs shared OSI linking

By default, OSIUtilities links against the **static position-independent** OSI
library (`open_simulation_interface_pic`). To link against the shared library
instead:

```bash
cmake ... -DLINK_WITH_SHARED_OSI=ON -DVCPKG_TARGET_TRIPLET=x64-linux-dynamic
```

Or use the `vcpkg-shared` preset with a dynamic triplet override:

```bash
cmake --preset vcpkg-shared -DBUILD_TESTING=ON -DVCPKG_TARGET_TRIPLET=x64-linux-dynamic
cmake --build --preset vcpkg-shared --parallel
```

> **Important:** When using shared OSI linking, protobuf (and its dependency
> abseil) must also be built as shared libraries. If protobuf is static, its code
> gets embedded in both the shared `libopen_simulation_interface.so` and the
> consumer binary, creating duplicate protobuf descriptor registrations that
> cause a fatal `"CheckTypeAndMergeFrom"` crash at runtime. Using a vcpkg dynamic
> triplet (e.g. `x64-linux-dynamic`, `arm64-osx-dynamic`) ensures all dependencies
> are shared.

#### When to use shared linking

Dynamic linking is required when **multiple libraries in the same process** each
need OSI/protobuf. If two libraries statically link their own copy of protobuf,
the duplicate global state (descriptor pools, generated message registries) causes
crashes or undefined behavior. The solution: both libraries link the same shared
OSI and protobuf libraries at runtime.

Typical scenario:
- Your application loads `libA.so` and `libB.so`
- Both use OSI messages
- Both must link against the **same** `libopen_simulation_interface.so` and
  `libprotobuf.so` to avoid duplicate registration

#### Platform support

| Platform | Shared OSI | Notes |
| --- | --- | --- |
| **Linux** | ✅ Supported | Set `LINK_WITH_SHARED_OSI=ON`. Tested in CI. |
| **macOS** | ✅ Supported | Set `LINK_WITH_SHARED_OSI=ON`. Tested in CI. |
| **Windows (MSVC)** | ⚠️ Not supported | Protobuf-generated code does not emit `__declspec(dllexport)` for data symbols. `WINDOWS_EXPORT_ALL_SYMBOLS` only covers functions, not global data like `_default_instance_`. See the [osi-cpp documentation](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/interface/setup/setting_up_osi_cpp.html) which marks Windows dynamic linking as "NOT RECOMMENDED". Use static linking (`_pic` target) on Windows. |

#### Runtime library discovery

When built with `LINK_WITH_SHARED_OSI=ON`, the shared OSI library must be
findable at runtime.

**Linux:**

```bash
# Option 1: Set LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/install/lib:$LD_LIBRARY_PATH
./my_app

# Option 2: Install to a system path and run ldconfig
sudo cp libopen_simulation_interface.so* /usr/local/lib/
sudo ldconfig

# Option 3: Use RPATH (set at build time — CMake does this by default for
# installed targets when CMAKE_INSTALL_RPATH is configured)
```

**macOS:**

```bash
export DYLD_LIBRARY_PATH=/path/to/install/lib:$DYLD_LIBRARY_PATH
./my_app
```

#### What the consumer CMake code looks like

The consumer-facing API is **identical** regardless of static or shared linking:

```cmake
find_package(OSIUtilities REQUIRED)
target_link_libraries(my_app PRIVATE OSIUtilities::OSIUtilities)
```

CMake handles the transitivity automatically. The only difference is whether the
OSI and protobuf symbols come from static archives or shared libraries at link
time.

---

## Dependency Visibility Reference

Understanding which dependencies are transitive helps when diagnosing build
issues or integrating into projects with existing dependency trees.

| Dependency | Visibility | In public API | Notes |
| --- | --- | --- | --- |
| **protobuf** | PUBLIC | `google::protobuf::Message` in `ReadResult`, `Descriptor*` in `AddChannel()` | Must be ABI-compatible between library and consumer |
| **OSI** (open_simulation_interface) | PUBLIC | Consumers instantiate `WriteMessage<osi3::GroundTruth>()` etc. | Built from proto source via osi-cpp submodule |
| **mcap** | PUBLIC (headers) | `mcap::McapWriterOptions`, `ReadMessageOptions`, `Metadata` in public methods | Header-only library, bundled |
| **lz4** | PRIVATE | Not in any public header | Linked internally for MCAP compression |
| **zstd** | PRIVATE | Not in any public header | Linked internally for MCAP compression |
| **abseil** | Transitive (via protobuf) | Not directly used | Required by protobuf ≥ 4.x at link time |
| **utf8_range** | Transitive (via protobuf) | Not directly used | Required by protobuf ≥ 4.x at link time |

### What this means for consumers

- You **do not** need to `find_package(Protobuf)` or `find_package(lz4)` — they
  are resolved automatically through the OSIUtilities CMake config.
- If your project already uses protobuf, ensure it is the **same version** used
  to build OSIUtilities. Mixing versions causes undefined behavior.
- If your project already uses lz4/zstd, there is no conflict — they are private
  to OSIUtilities and will not pollute your include paths.

---

## Troubleshooting

### `find_package(OSIUtilities)` fails

Ensure `CMAKE_PREFIX_PATH` includes the install prefix:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/deps
```

### Protobuf not found (during `find_package(OSIUtilities)`)

The OSIUtilities config calls `find_dependency(Protobuf)` internally. If this
fails, protobuf is not findable from your build environment. Solutions:

- **Two-phase build:** Pass `VCPKG_INSTALLED_DIR` and `VCPKG_MANIFEST_MODE=OFF`
  so the vcpkg toolchain finds the pre-installed protobuf.
- **System packages:** Ensure `libprotobuf-dev` (or equivalent) is installed.
- **Manual:** Set `Protobuf_DIR` or `CMAKE_PREFIX_PATH` to your protobuf install.

### Abseil linker errors

If you see unresolved `absl::` symbols, protobuf was likely found via CMake's
built-in `FindProtobuf` module (which doesn't know about abseil) instead of
protobuf's own config-mode package. Fix: use the vcpkg toolchain or explicitly
request config mode:

```cmake
find_package(Protobuf CONFIG REQUIRED)  # before find_package(OSIUtilities)
```

### Symbol conflicts with existing protobuf

If your project links a different protobuf version, you will get linker errors
or runtime crashes. Ensure a single protobuf version across the entire build.
The two-phase build (Method A) is safest because it isolates the dependency
resolution.

---

## Next Steps

- [Building](building.md) — Build the library itself
- [Examples](examples.md) — Code examples for readers and writers
- [API Reference](cpp/index.rst) — Full C++ API documentation
