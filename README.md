# Blaze Engine

Windows **x64** 2D game engine — OpenGL / ImGui editor (**Sandbox**), **Lua** scripting, and a packaged **Runtime** player.

## Quick start

### Run editor (Debug)

```powershell
cd Sandbox
..\bin\Debug-windows-x86_64\Sandbox\Sandbox.exe
```

Working directory must be `Sandbox` (or your game project folder) so `content/` loads.

### Build Debug

```powershell
msbuild Blaze.sln /p:Configuration=Debug /p:Platform=x64
```

### Build Release + offline package

```powershell
powershell -File scripts\PrepareInstaller.ps1 -Build
```

Creates `dist\Blaze-Windows-x64.zip` (`Blaze.exe`, `Blaze.dll`, `Runtime.exe`, docs).  
Users unzip and run `Blaze.exe` — no Visual Studio required.

## Docs

Full reference: **[DOCUMENTATION.md](DOCUMENTATION.md)**

## Targets

| Project | Output | Role |
|---------|--------|------|
| Blaze | `Blaze.dll` | Engine |
| Sandbox | `Sandbox.exe` | Editor |
| Runtime | `Runtime.exe` | Ship games |

## Requirements

- Windows 64-bit
- Visual Studio with C++ desktop workload (to build from source)
