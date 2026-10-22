# Windows routing core

KokoroBox uses only `ProxyBridgeCore.dll` together with its own authenticated
`kokorobox-process-router.exe` sidecar. This repository does not build or ship
a ProxyBridge GUI, CLI, or installer.

## Build

Install Visual Studio C++ x64 tools and WinDivert 2.2.2-A, then run:

```powershell
cd Windows
.\compile.ps1
```

The output directory contains only `ProxyBridgeCore.dll`, `WinDivert.dll`, and
`WinDivert64.sys`. KokoroBox's release build compiles the same core directly at
its pinned revision and supplies the process router itself.

The fork's Windows version is defined in `src/pb_version.h` (currently
`4.26.10`, numeric file version `4.26.10.0`). The DLL's Properties → Details
identifies it as **ProxyBridge for KokoroBox** and records the source Git commit
in `PrivateBuild`, with a `-dirty` suffix for modified Windows sources. This
version is independent of upstream releases and the macOS extension version.

`compile.ps1` compiles and links the version resource, then verifies the DLL's
metadata with `test-version-info.ps1`. External build pipelines can use
`build-version-resource.ps1 -VcVars <vcvarsall.bat> -OutputPath <core.res>`
and link the resulting resource into the core DLL, then run the same check.
