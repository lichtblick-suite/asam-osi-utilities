# Rust MCAP in C++: Strategy

**Goal**
Use the Rust MCAP implementation to gain finer control over chunk boundaries (e.g., flush/rotate chunks based on data volume per frame) while keeping the existing C++ conversion workflow.

**Feasibility**
This is practical: Rust can build a `staticlib` or `cdylib` and expose a C ABI that C++ can call. The C ABI approach keeps the boundary simple and minimizes template/ABI friction.

**Recommended Approach: C ABI Wrapper (Most Practical)**

1. Create a small Rust crate (for example `lib/mcap-rs-ffi`) that depends on the Rust `mcap` crate.
1. Expose a minimal `extern "C"` API for writer lifecycle and chunk control.
1. Generate a C header using `cbindgen` and check it into the repo for stable builds.
1. Build the Rust crate with `cargo build --release` to produce a `.a` or `.so`.
1. Integrate the build in CMake using `ExternalProject_Add` or `FetchContent`, then link the resulting library into the C++ targets.
1. Add a thin C++ RAII wrapper in `src/tracefile/writer/` that maps C ABI handles and errors into C++ exceptions or status codes.
1. Gate the Rust path behind a CMake option (for example `OSIUTILITIES_USE_RUST_MCAP`) to keep the current C++ writer as the default.
1. Add CI coverage to build both variants (C++-only and Rust-backed) to catch toolchain or ABI issues early.

**Minimal C ABI Surface (Example Capabilities)**

1. Create/destroy writer handle.
1. Set chunking policy (fixed size or explicit flush control).
1. Add schema and channel metadata.
1. Write message records with timestamp, channel id, and payload.
1. Flush current chunk and finalize the file.

**Alternative Approaches**

1. CXX bridge (`cxx` crate). This gives a more ergonomic interface but increases build complexity and may limit API shapes.
1. Sidecar process. Run a Rust converter as a separate executable and communicate via pipes or temporary files. This is quick to integrate but adds operational complexity and overhead.

**Risks and Mitigations**

1. Build complexity and cross-compilation. Mitigate with pinned Rust toolchains and CI checks for all target platforms.
1. ABI stability. Keep the C surface minimal and versioned.
1. Ownership and error handling. Use explicit destroy functions and error codes; avoid passing C++ objects across the boundary.

**Validation Plan**

1. Convert the OSI example traces and compare chunk counts, average chunk size (uncompressed), and playback smoothness.
1. Validate output with `mcap` tooling and the existing MCAP reader.
1. Compare compression ratios and conversion time with the current C++ writer.
