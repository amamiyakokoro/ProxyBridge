# ProxyBridge for KokoroBox

This independently maintained repository supplies the application-routing
components used by [KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop).
It is not a standalone ProxyBridge distribution. Its supported surface is
limited to the Windows core DLL and the KokoroBox macOS system extension.

## Scope

KokoroBox pins this repository by commit and reuses only these components:

| Platform | Reused component | Integration |
| --- | --- | --- |
| Windows 10/11 x64 | `ProxyBridgeCore.dll` | A controlled process-router sidecar installs executable-path rules and routes selected applications through KokoroBox's local Mihomo SOCKS5 listener. |
| macOS 13+ | `NETransparentProxyProvider` system extension | KokoroBox atomically installs signing-identifier rules and routes selected applications through the fixed `127.0.0.1:7891` SOCKS5 endpoint. |

This repository intentionally contains no GUI, updater, CLI, installer, DNS
proxy provider, or Linux implementation.

## KokoroBox changes

- `PROXY` decisions fail closed: unavailable or invalid proxy configuration
  becomes `BLOCK`, never `DIRECT`.
- Windows adds opt-in, per-application UDP/53 routing and room for atomic
  rule replacement.
- macOS accepts an atomic signing-identifier policy for the fixed local SOCKS5
  endpoint and excludes KokoroBox, Mihomo, and local/control traffic to avoid
  loops.

See [KOKOROBOX.md](KOKOROBOX.md) for the complete integration contract.

## Build

To build the Windows core on Windows 10/11 x64 with MSVC and WinDivert 2.2.2:

```powershell
cd Windows
.\compile.ps1 -Compiler msvc -NoSign
```

KokoroBox builds the macOS system extension from
`MacOS/ProxyBridge/ProxyBridge.xcodeproj` using
`MacOS/ProxyBridge/kokorobox-ext.xcconfig`; its release pipeline supplies the
architecture and signing settings. Build output is verified and signed by the
KokoroBox release pipeline.

## License and attribution

ProxyBridge is licensed under the [MIT License](LICENSE). Copyright and
attribution from the original project are retained. The Windows
implementation uses [WinDivert](https://reqrypt.org/windivert.html), whose
license and redistribution requirements must also be preserved.
