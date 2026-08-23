# Building Waywallen

Lito is the only supported project build entry. CMake is still required as a host tool because
Lito uses CMake providers for Qt and source dependencies, but this repository is not configured or
built with CMake directly.

## Requirements

- Lito
- Clang 22 or newer
- Rust and Cargo
- CMake and Ninja for Lito external dependencies
- Qt 6 with Quick, DBus, Protobuf, ProtobufQuick, QuickControls2, and WebSockets
- Vulkan headers and loader
- GBM and the FFmpeg development packages used by the renderer plugins

## Build and install

From the repository root:

```bash
lito build --profile debug
lito install --prefix install --profile debug
```

Use `--profile release` for release artifacts. The default workspace members install the daemon,
UI, renderer plugins, bridge library, public bridge headers, and pkg-config metadata into the same
prefix.

The bridge tests are Lito targets:

```bash
lito test -p waywallen-bridge --profile debug
```

## Run from a prefix

```bash
cd install
export QML_IMPORT_PATH="$PWD/lib/qt6/qml"
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/waywallen --ui ./bin/waywallen-ui --plugin ./share/waywallen
```

Packaging should consume the tree produced by `lito install`; there is no CMake or CPack project
interface.
