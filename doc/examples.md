# Examples

This project ships small executables that demonstrate how to read/write OSI trace files and convert between formats. Use them as reference implementations for API usage and expected file formats.

## What the examples cover

- `example_mcap_reader` / `example_mcap_writer` — MCAP read/write
- `example_single_channel_binary_reader` / `example_single_channel_binary_writer` — `.osi` binary read/write
- `example_txth_reader` / `example_txth_writer` — `.txth` text read/write
- `convert_osi2mcap` — convert `.osi` to `.mcap`

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

For more details, see [examples/README.md](../examples/README.md).
