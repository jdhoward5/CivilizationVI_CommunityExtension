# Third-Party Dependencies

This project uses the following third-party libraries:

## Vendored (included in repository)

### asmjit
- **Description**: JIT assembler for x86/x64 architectures
- **Version**: 1.21.x (latest)
- **License**: Zlib
- **Source**: https://github.com/asmjit/asmjit
- **Location**: `asmjit/`
- **Notes**: Full source included; built as part of the project

### Capstone (headers only)
- **Description**: Disassembly framework
- **Version**: 6.0.0
- **License**: BSD-3-Clause
- **Source**: https://github.com/capstone-engine/capstone
- **Location**: `include/capstone/`
- **Notes**: Only headers included; library must be built separately (see Build Instructions)

### MinHook (header only)
- **Description**: Minimalistic x86/x64 API hooking library
- **Version**: 1.3.3
- **License**: BSD-2-Clause
- **Source**: https://github.com/TsudaKageyu/minhook
- **Location**: `include/MinHook.h`
- **Notes**: Only header included; library must be built separately (see Build Instructions)

### nlohmann/json (header only)
- **Description**: JSON for Modern C++
- **Version**: 3.x
- **License**: MIT
- **Source**: https://github.com/nlohmann/json
- **Location**: `include/nlohmann/`
- **Notes**: Single-header library, fully included

## Build Dependencies (not included)

The following libraries must be built and placed in `lib/Release/` before linking:

| Library | Filename | Build Instructions |
|---------|----------|-------------------|
| MinHook | `libMinHook.x64.lib` | Clone [minhook](https://github.com/TsudaKageyu/minhook), build `build/VC17/libMinHook.vcxproj` (Release x64) |
| Capstone | `capstone.lib` | Clone [capstone](https://github.com/capstone-engine/capstone), run `cmake -B build -A x64 && cmake --build build --config Release` |

## License Summary

| Library | License |
|---------|---------|
| asmjit | Zlib |
| Capstone | BSD-3-Clause |
| MinHook | BSD-2-Clause |
| nlohmann/json | MIT |

All licenses are permissive and compatible with AGPL-3.0.
