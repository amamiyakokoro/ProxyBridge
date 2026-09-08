# ProxyBridge Windows core for KokoroBox

This fork provides `ProxyBridgeCore.dll` for
[KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop)
application routing on Windows 10/11 x64. It is not a standalone Windows
ProxyBridge release and does not distribute the upstream GUI, CLI, updater, or
installer.

## Runtime

KokoroBox builds this core at a pinned commit and loads it through its
`kokorobox-process-router.exe` sidecar. The sidecar owns the command protocol,
the fixed local Mihomo SOCKS5 endpoint, and mandatory loop-prevention rules.

- `PROXY` fails closed: missing, invalid, or UDP-incompatible proxy settings
  resolve to `BLOCK`, never `DIRECT`.
- Per-application UDP/53 routing is opt-in and does not widen other UDP rules.
- Rule storage supports atomic replacement of a bounded set of canonical
  executable paths.

KokoroBox packages only the sidecar, `ProxyBridgeCore.dll`, the unmodified
WinDivert runtime, and their required license notices. See
[KOKOROBOX.md](../KOKOROBOX.md) for the integration contract.

## Build

Requirements: Windows 10/11 x64, the MSVC C++ toolchain, and WinDivert 2.2.2
extracted to `C:\WinDivert-2.2.2-A`.

```powershell
cd Windows
.\compile.ps1 -Compiler msvc -NoSign
```

The output is written to `Windows/output/`. KokoroBox verifies pinned inputs
and signs its distributable binaries in its own release pipeline. Do not modify
or re-sign the official WinDivert driver.

## Upstream and license

For standalone ProxyBridge documentation, use
[README.upstream.md](../README.upstream.md). This fork retains the upstream
[MIT License](../LICENSE) and attribution; WinDivert's redistribution terms
also apply.
