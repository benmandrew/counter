# Counter

Project for $k$-bounded counting of traces that satisfy FRET requirements using transfer matrices.

## Build and Run

```sh
cmake -S . -B build
cmake --build build
./build/counter
```

### Dependencies

| Dependency | Link |
|---|---|
| Ganak | https://github.com/meelgroup/ganak |

Ganak only has release binaries for Linux and MacOS, but the `CMakeLists.txt` makes reference to Windows, so it may be possible to compile for Windows.

## Format, Lint, and Test

```sh
cmake -S . -B build
cmake --build build --target format lint tests
```
