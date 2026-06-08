## Summary

<!-- What changed and why? -->

## Test plan

- [ ] Built locally (`cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DREMUS_ENABLE_WARNINGS=ON && cmake --build build`)
- [ ] Release config still builds with warnings (`cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DREMUS_ENABLE_WARNINGS=ON && cmake --build build-rel`)
- [ ] Ran tests (`ctest --test-dir build --output-on-failure`)
- [ ] Formatted C++ if sources changed (clang-format version in `.clang-format-version`)
- [ ] Added or updated tests where behavior changed
- [ ] GUI changes: `ctest --test-dir build -R GuiControllersSmokeTest --output-on-failure` (and/or manual `./build/src/gui/remus-gui` smoke)

## Notes

<!-- Breaking changes, follow-ups, or reviewer callouts -->
