# Tool Usage Patterns — Remus

| Tool / command | Effective usage pattern |
|----------------|-------------------------|
| `cd build && ctest --output-on-failure` | Run after every change; treat red as blocking |
| `cmake --build build 2>&1 \| grep -c ": error:"` | Run after every type definition change |
| `cd build && ctest --output-on-failure && cmake --build build && find src -name '*.cpp' -o -name '*.h' -o -name '*.qml' \| xargs wc -l \| tail -1` | Three-check ritual — run before marking a task done |
| `find src -name '*.cpp' -o -name '*.h' -o -name '*.qml' \| xargs wc -l \| tail -1` | LOC baseline check |

## Extension registry

| Stack signals | Recommended extensions |
|--------------|------------------------|
| `CMakeLists.txt`, `*.cmake` | `ms-vscode.cmake-tools` · `twxs.cmake` |
| `*.cpp`, `*.h`, `*.hpp` | `ms-vscode.cpptools` · `llvm-vs-code-extensions.vscode-clangd` |
| `*.qml` | `qt-official.qt-cpp-tools` |

*(Updated as effective workflows are discovered.)*
