# Civilization VI: Community Extension (Fork)

This is a fork of the [original CivilizationVI_CommunityExtension](https://github.com/Wild-W/CivilizationVI_CommunityExtension) by [Wild-W](https://github.com/Wild-W).

Report bugs on the [issue tracker](https://github.com/jdhoward5/CivilizationVI_CommunityExtension/issues).

[Visit the original wiki!](https://github.com/Wild-W/CivilizationVI_CommunityExtension/wiki)

## Building

### Requirements
- Visual Studio 2022 (MSVC v143)
- Windows SDK
- C++17 support

### Dependencies

This project depends on the following libraries (see [THIRD_PARTY.md](THIRD_PARTY.md) for details):

| Library | Included | Notes |
|---------|----------|-------|
| [asmjit](https://github.com/asmjit/asmjit) | Yes | Full source vendored |
| [Capstone](https://github.com/capstone-engine/capstone) | Headers only | Must build library |
| [MinHook](https://github.com/TsudaKageyu/minhook) | Header only | Must build library |
| [nlohmann/json](https://github.com/nlohmann/json) | Yes | Header-only |

### Quick Build (Recommended)

Use the included PowerShell build script:

```powershell
.\build.ps1              # Build Release (default)
.\build.ps1 -Clean       # Clean and rebuild
.\build.ps1 -Debug       # Build Debug configuration
.\build.ps1 -BuildDeps   # Force rebuild dependencies
.\build.ps1 -Help        # Show all options
```

The script automatically clones and builds MinHook and Capstone if needed.

### Manual Build Steps

<details>
<summary>Click to expand manual instructions</summary>

1. **Clone the repository**
   ```
   git clone https://github.com/jdhoward5/CivilizationVI_CommunityExtension.git
   cd CivilizationVI_CommunityExtension
   ```

2. **Build MinHook**
   ```
   git clone https://github.com/TsudaKageyu/minhook.git
   msbuild minhook/build/VC17/libMinHook.vcxproj /p:Configuration=Release /p:Platform=x64
   mkdir lib\Release
   copy minhook\build\VC17\lib\Release\libMinHook.x64.lib lib\Release\
   ```

3. **Build Capstone**
   ```
   git clone https://github.com/capstone-engine/capstone.git
   cd capstone
   cmake -B build -A x64
   cmake --build build --config Release
   cd ..
   copy capstone\build\Release\capstone.lib lib\Release\
   ```

4. **Build the project**
   ```
   msbuild CivilizationVI_CommunityExtension.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

</details>

The output DLL will be placed in:
```
%USERPROFILE%\Documents\My Games\Sid Meier's Civilization VI\Mods\CivilizationVI_CommunityExtension_Mod\Binaries\Win64\
```

## FAQ

### What does this mod do?
This mod exposes new tools for modders to use by editing Civ VI's base code.

### How does this mod edit the base code?
In essence, the game will load a custom DLL, which then in turn loads the GameCore DLL and makes changes to it. If you want to learn more, join the [Civilization VI Modding Helpline](https://discord.gg/jSVhyBYvZR).

### Is this mod compatible with Mac/Linux?
No. It's possible to port parts of it, but it's just too far out of scope right now.

## License
This project is distributed under AGPL 3.0. That means that **all** derivative works of this project **must be open source** and **distributed under the same AGPL 3.0 license**. This is to protect the modding community from bad actors and encourage collaboration.

## Credits
- Original project by [Wild-W](https://github.com/Wild-W)
- Third-party libraries listed in [THIRD_PARTY.md](THIRD_PARTY.md)
