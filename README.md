# ProxyBridge for KokoroBox

This repository is the maintained ProxyBridge fork used by
[KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop) for
application-based routing on Windows and macOS.

It is not the standalone ProxyBridge distribution. The original project,
documentation, downloads, and screenshots are available from
[InterceptSuite/ProxyBridge](https://github.com/InterceptSuite/ProxyBridge).
The README inherited from the pinned upstream revision is preserved in
[README.upstream.md](README.upstream.md).

## Purpose

KokoroBox pins this repository by commit and reuses only the platform routing
components it needs:

| Platform | Reused component | Integration |
| --- | --- | --- |
| Windows 10/11 x64 | `ProxyBridgeCore.dll` | A controlled process-router sidecar installs executable-path rules and routes selected applications through KokoroBox's local Mihomo SOCKS5 listener. |
| macOS 13+ | `NETransparentProxyProvider` system extension | KokoroBox atomically installs signing-identifier rules and routes selected applications through the fixed `127.0.0.1:7891` SOCKS5 endpoint. |

Rules can send matching traffic through the proxy, allow it to connect
directly, or block it. The upstream GUIs, updater, Windows CLI, macOS DNS proxy
provider, and Linux implementation are not shipped as part of KokoroBox.

## Fork changes

### Windows

- `PROXY` rules fail closed: missing, invalid, or UDP-incompatible proxy
  configuration resolves to `BLOCK`, never `DIRECT`.
- An explicit API controls whether an application's UDP/53 DNS queries
  participate in `PROXY` rules, including TCP-only rules, without widening any
  other UDP routing.
- The process-rule storage limit is increased so KokoroBox can atomically
  install a bounded set of canonical executable paths and mandatory
  loop-prevention exclusions.

### macOS

- A versioned provider message atomically replaces a bounded set of
  signing-identifier rules and the fixed local SOCKS5 endpoint.
- Controlled `PROXY` decisions fail closed whenever the local proxy is
  unavailable.
- KokoroBox, its helper and extension, Mihomo, loopback, link-local, multicast,
  and broadcast traffic are permanently excluded to prevent routing loops.
- KokoroBox-specific bundle metadata and minimal Network Extension
  entitlements are isolated from the upstream standalone configuration.

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

## Building the macOS extension

Requirements:

- macOS 13 or later
- Xcode with the macOS SDK
- A signing identity, provisioning profile, and Network Extension entitlement
  suitable for distributing a system extension

The extension target is in `MacOS/ProxyBridge/ProxyBridge.xcodeproj`.
KokoroBox's release pipeline builds that target with
`MacOS/ProxyBridge/kokorobox-ext.xcconfig`, supplies `KOKOROBOX_TARGET_ARCH`
and the signing settings, then embeds the resulting system extension in the
KokoroBox app. The configuration registers only the transparent app-proxy
provider; it does not enable the upstream DNS proxy provider.

For the complete standalone build and usage documentation, refer to
[README.upstream.md](README.upstream.md) and the platform-specific README
files.

## Upstream synchronization

Keep KokoroBox-specific changes as small and reviewable as possible. When
syncing upstream, verify the fail-closed behavior and loop exclusions, build
each affected platform component, and update KokoroBox Desktop's pinned commit
only after those checks pass.

## License and attribution

ProxyBridge is licensed under the [MIT License](LICENSE). Copyright and
attribution from the upstream project are retained. The Windows
implementation uses [WinDivert](https://reqrypt.org/windivert.html), whose
license and redistribution requirements must also be preserved.
