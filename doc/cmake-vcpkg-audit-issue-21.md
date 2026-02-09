<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# CMake and vcpkg Audit (Issue #21)

Audit date: 2026-02-09

## Implementation Status (Branch: `fix/build-issues`)

- Implemented:
  - vcpkg CI test-directory fix (`build-vcpkg` for vcpkg jobs)
  - docs command alignment for vcpkg test directories
  - explicit recursive submodule validation for `lib/osi-cpp/open-simulation-interface`
  - windows static-md preset (`vcpkg-windows-static-md`) and CI usage
  - CI linkage diagnostics/guards for vcpkg jobs
  - documented policy and linkage expectations in `doc/building.md`
- Intentional:
  - Ubuntu `base`/system-package job remains as a compatibility path and may link shared system protobuf.

## Scope

- Top-level CMake and vcpkg configuration (`CMakeLists.txt`, `CMakePresets.json`, `vcpkg*.json`)
- Submodule build integration (`lib/osi-cpp`, `lib/mcap`)
- Recursive submodule content used by `osi-cpp` (`lib/osi-cpp/open-simulation-interface`) and both CMake entry points in that subtree
- CI dependency/install and build/test workflows (`.github/dependencies.yml`, `.github/workflows/*.yml`)
- Issue context:
  - https://github.com/lichtblick-suite/asam-osi-utilities/issues/21
  - https://github.com/lichtblick-suite/asam-osi-utilities/issues/21#issuecomment-3855320327
  - https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/interface/setup/setting_up_osi_cpp.html

## Executive Summary

The concern in issue #21 is valid for the current Ubuntu `base` pipeline: it builds `open_simulation_interface_pic` (static PIC OSI) but still links final binaries against shared system protobuf (`libprotobuf.so`). This is exactly the "common error" called out in the ASAM OSI docs.

The `vcpkg` preset behaves differently on Linux and links static protobuf archives (`libprotobuf.a`), which aligns better with static packaging goals.

The nested `lib/osi-cpp/open-simulation-interface` submodule is part of the effective build inputs (version/proto sources). However, its own `CMakeLists.txt` is a documentation project and is not part of the main library build graph.

In addition, current CI has a path mismatch in all vcpkg jobs: configure/build use `build-vcpkg`, but tests are executed from `build`.

## Findings

### F1 (High): Ubuntu `base` path matches the issue #21 risk (shared protobuf linkage)

Evidence:

- System package job installs shared protobuf development packages:
  - `.github/dependencies.yml:158`
  - `.github/dependencies.yml:164`
- Ubuntu CI job configures with `cmake --preset base`:
  - `.github/workflows/ci_build.yml:46`
- `base` preset is explicitly non-vcpkg:
  - `CMakePresets.json:5`
- `base` build cache resolves protobuf to shared objects:
  - `build/CMakeCache.txt` (`Protobuf_LIBRARY_RELEASE=/usr/lib/x86_64-linux-gnu/libprotobuf.so`)
- Built executable link command includes shared protobuf:
  - `build/examples/CMakeFiles/example_mcap_writer.dir/link.txt` contains `/usr/lib/x86_64-linux-gnu/libprotobuf.so`
- ASAM OSI docs explicitly say this path is not recommended for static packaging and can produce static-OSI + dynamic-protobuf mixing.

Impact:

- For redistributable binaries/FMUs, this increases runtime dependency fragility and portability risk.
- The issue discussion concern is accurate for the current Ubuntu `base` CI path.

### F2 (High): vcpkg CI jobs run tests from the wrong build directory

Evidence:

- `vcpkg` preset binary dir is `build-vcpkg`:
  - `CMakePresets.json:16`
- All vcpkg workflows run ctest in `build`:
  - `.github/workflows/ci_build.yml:89`
  - `.github/workflows/ci_build.yml:115`
  - `.github/workflows/ci_build.yml:150`

Impact:

- Tests may run from an unrelated/nonexistent tree in clean CI jobs.
- Reported vcpkg test coverage can be wrong or brittle.

### F3 (Medium): Windows static-linking intent is not enforced by triplet configuration

Evidence:

- No preset sets `VCPKG_TARGET_TRIPLET`:
  - `CMakePresets.json:1`
- OSI docs recommend `x64-windows-static-md` when static protobuf is required for distribution use cases.

Impact:

- Windows vcpkg builds may not consistently match intended static packaging behavior.

### F4 (Medium): Documentation and workflow commands still point to `build` for vcpkg test runs

Evidence:

- `doc/workflow.md:24` and `doc/workflow.md:25` use `cmake --preset vcpkg` and then `ctest --test-dir build`.
- Same mismatch exists in CI workflow docs:
  - `doc/ci-pipeline.md:170`
  - `doc/ci-pipeline.md:171`

Impact:

- Developer and CI docs reinforce the wrong test directory pattern.

### F5 (Low): Strategy ambiguity between "manual system builds" and "static packaging guidance"

Evidence:

- Project docs keep both modes:
  - `doc/building.md:35` (vcpkg recommended)
  - `doc/building.md:146` (manual/system path still documented)
- Current CI still validates both system-package and vcpkg package paths.

Impact:

- Without explicit policy, "dynamic allowed for CI smoke" and "static required for distributables" are easy to conflate.

### F6 (Medium): `open-simulation-interface` sub-submodule is critical input, but not explicitly validated at top level

Evidence:

- Recursive submodule wiring exists in `osi-cpp`:
  - `lib/osi-cpp/.gitmodules:1`
- Top-level only checks for `lib/osi-cpp/CMakeLists.txt`, not nested sub-submodule content:
  - `CMakeLists.txt:64`
  - `CMakeLists.txt:65`
  - `CMakeLists.txt:66`
- `osi-cpp` directly consumes files from `open-simulation-interface` (thus requiring recursive submodule content):
  - `lib/osi-cpp/CMakeLists.txt:23`
  - `lib/osi-cpp/CMakeLists.txt:66`
  - `lib/osi-cpp/CMakeLists.txt:70`

Impact:

- If the sub-submodule is missing or out of sync, configuration fails later inside `osi-cpp` with less direct guidance than the top-level check currently provides.

### F7 (Low): `open-simulation-interface` CMake in sub-submodule is docs-only and not part of the library build graph

Evidence:

- Main OSI library targets are created in `lib/osi-cpp/CMakeLists.txt`:
  - `lib/osi-cpp/CMakeLists.txt:76`
  - `lib/osi-cpp/CMakeLists.txt:105`
  - `lib/osi-cpp/CMakeLists.txt:122`
- The nested file is a separate documentation project:
  - `lib/osi-cpp/open-simulation-interface/CMakeLists.txt:3`
- There is no `add_subdirectory(open-simulation-interface)` in `lib/osi-cpp/CMakeLists.txt`.

Impact:

- Changes in `lib/osi-cpp/open-simulation-interface/CMakeLists.txt` do not influence library linking behavior for this project.
- Review focus for issue #21 should stay on `lib/osi-cpp/CMakeLists.txt` and the selected dependency source (system packages vs vcpkg).

## Recommendations

1. Decide and document policy explicitly:
   - `CI/system-package` jobs are compatibility checks only (dynamic deps acceptable), or
   - all release-relevant jobs must be static-consistent (prefer vcpkg-only for release quality gates).
2. Fix vcpkg CTest directory mismatch in CI (`build-vcpkg`).
3. Add explicit Windows triplet in preset(s) for intended linkage:
   - e.g. `VCPKG_TARGET_TRIPLET=x64-windows-static-md` for static libs + dynamic CRT.
4. Align documentation commands with presets:
   - `ctest --test-dir build-vcpkg` after `cmake --preset vcpkg`.
5. Add explicit recursive-submodule validation for `lib/osi-cpp/open-simulation-interface` at configure time with a direct remediation message (`git submodule update --init --recursive`).
6. Optional hardening:
   - add a CI check (or CMake message summary) to print resolved protobuf library type/path for each preset.

## Proposed Implementation Plan

### Phase 1: Correctness and transparency (small, immediate)

- Update CI test directories in `.github/workflows/ci_build.yml` for vcpkg jobs.
- Update docs (`doc/workflow.md`, `doc/ci-pipeline.md`) to use correct vcpkg test directory.
- Add one short section in `doc/building.md` clarifying:
  - `base/system` may link shared protobuf,
  - `vcpkg` is the recommended path for static-friendly packaging.
- Add a configure-time check that `lib/osi-cpp/open-simulation-interface/VERSION` (and/or `osi_version.proto.in`) exists before proceeding, with an explicit recursive-submodule hint.

Acceptance criteria:

- vcpkg jobs configure/build/test from one consistent tree (`build-vcpkg`).
- Local docs no longer instruct running vcpkg tests from `build`.
- Missing recursive submodule content fails fast with a clear action message.

### Phase 2: Linkage policy enforcement (medium)

- Add explicit windows triplet preset(s), for example:
  - `vcpkg-windows-static-md`
  - optional dedicated Linux/macOS vcpkg presets if desired for clarity.
- Decide whether Ubuntu `base` remains required:
  - keep as compatibility check, or
  - move release gating to vcpkg-only jobs.

Acceptance criteria:

- Presets make intended linkage model explicit by platform.
- CI matrix description matches actual release intent.

### Phase 3: Guardrails (optional but recommended)

- Add a small CI verification step that inspects resolved protobuf linkage (from `CMakeCache.txt` or link lines) and fails on unexpected modes for specific jobs.
- Add a short "linkage expectations" table to docs.

Acceptance criteria:

- Linkage regressions are caught early by CI.
- Contributors can quickly verify which jobs are expected to be static or dynamic.

## Notes and Limitations

- This audit used direct file inspection, CI definitions, and existing build artifacts.
- A full rerun of `cmake --preset vcpkg` in this session required elevated access to the shared vcpkg lock path and was not approved, so conclusions for vcpkg are based on current checked-in config plus existing local `build-vcpkg` artifacts.
