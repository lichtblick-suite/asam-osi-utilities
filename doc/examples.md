# Examples

This project ships small executables that demonstrate how to read/write OSI trace files and convert between formats. Use them as reference implementations for API usage and expected file formats.

## What the examples cover

- [example_mcap_reader.cpp](@ref example_mcap_reader.cpp) — MCAP read ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/example_mcap_reader.cpp))
- [example_mcap_writer.cpp](@ref example_mcap_writer.cpp) — MCAP write ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/example_mcap_writer.cpp))
- [example_single_channel_binary_reader.cpp](@ref example_single_channel_binary_reader.cpp) — `.osi` binary read ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/example_single_channel_binary_reader.cpp))
- [example_single_channel_binary_writer.cpp](@ref example_single_channel_binary_writer.cpp) — `.osi` binary write ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/example_single_channel_binary_writer.cpp))
- [example_txth_reader.cpp](@ref example_txth_reader.cpp) — `.txth` text read ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/example_txth_reader.cpp))
- [example_txth_writer.cpp](@ref example_txth_writer.cpp) — `.txth` text write ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/example_txth_writer.cpp))
- [convert_osi2mcap.cpp](@ref convert_osi2mcap.cpp) — convert `.osi` to `.mcap` ([source](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/convert_osi2mcap.cpp))

## Build examples

Examples are built as part of the normal build:

```bash
cmake --preset vcpkg-linux    # or vcpkg-windows / vcpkg
cmake --build build -j
```

## Run examples

Binaries are in `build/examples/`.

```bash
# Example
./build/examples/example_mcap_reader input.mcap
```

For more details, see [examples/README.md](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/examples/README.md).
