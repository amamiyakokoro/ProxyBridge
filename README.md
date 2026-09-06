# ProxyBridge for KokoroBox

This repository is the maintained ProxyBridge fork used by
[KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop) for
Windows application-based routing.

It is not the standalone ProxyBridge distribution. The original project,
documentation, downloads, and screenshots are available from
[InterceptSuite/ProxyBridge](https://github.com/InterceptSuite/ProxyBridge).
The README inherited from the pinned upstream revision is preserved in
[README.upstream.md](README.upstream.md).

## Purpose

KokoroBox builds `ProxyBridgeCore.dll` from a pinned commit of this fork and
uses it behind a controlled process-router sidecar. Selected Windows
applications can be routed through KokoroBox's local Mihomo SOCKS5 listener,
allowed to connect directly, or blocked.

The initial integration targets Windows 10/11 x64. The upstream macOS and
Linux sources remain in the repository but are not part of the KokoroBox MVP.

## Fork changes

- `PROXY` rules fail closed: missing, invalid, or UDP-incompatible proxy
  configuration resolves to `BLOCK`, never `DIRECT`.
- The process-rule storage limit is increased so KokoroBox can atomically
  install a bounded set of canonical executable paths and mandatory
  loop-prevention exclusions.
- KokoroBox pins this repository by commit for reproducible builds and future
  integration-specific development.

See [KOKOROBOX.md](KOKOROBOX.md) for the current integration contract.

## Building the Windows core

Requirements:

- Windows 10/11 x64
- Visual Studio with the MSVC C++ toolchain
- WinDivert 2.2.2 extracted to `C:\WinDivert-2.2.2-A`

From PowerShell:

```powershell
cd Windows
.\compile.ps1 -Compiler msvc -NoSign
```

Build output is written to `Windows/output/`. KokoroBox's release pipeline is
responsible for verifying pinned inputs and signing its distributable
binaries. Do not modify or re-sign the official WinDivert driver.

For the complete standalone build and usage documentation, refer to
[README.upstream.md](README.upstream.md) and the platform-specific README
files.

## Upstream synchronization

Keep KokoroBox-specific changes as small and reviewable as possible. When
syncing upstream, verify the fail-closed behavior and run the Windows build
before updating the commit pinned by KokoroBox Desktop.

## License and attribution

ProxyBridge is licensed under the [MIT License](LICENSE). Copyright and
attribution from the upstream project are retained. The Windows
implementation uses [WinDivert](https://reqrypt.org/windivert.html), whose
license and redistribution requirements must also be preserved.
