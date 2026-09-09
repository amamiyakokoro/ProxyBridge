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
