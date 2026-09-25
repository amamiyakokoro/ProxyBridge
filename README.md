<div align="center">

# ProxyBridge for KokoroBox

Windows and macOS application-routing components used by [KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop).

[Integration](KOKOROBOX.md) · [Security](SECURITY.md) · [License](LICENSE)

</div>

## Features

- Windows `ProxyBridgeCore.dll` routes selected applications through KokoroBox's local Mihomo SOCKS5 listener using executable-path rules and optional per-application UDP/53 routing.
- The macOS `NETransparentProxyProvider` system extension uses signing-identifier or process-name rules and the fixed `127.0.0.1:7891` SOCKS5 endpoint.
- `PROXY` decisions resolve to `BLOCK` when the proxy configuration or service is unavailable, and both integrations exclude traffic that could cause routing loops.

## Supported platforms

| Platform | Component |
| --- | --- |
| Windows 10/11 x64 | `ProxyBridgeCore.dll`, used with KokoroBox's process-router sidecar |
| macOS 13+ | `NETransparentProxyProvider` system extension |

This repository is pinned by KokoroBox at an exact commit. It does not provide a standalone application, installer, updater, CLI, DNS proxy provider, or Linux implementation.

## Get started

Download [KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop/releases) to use application routing. These components are built and distributed through its release pipeline.

## Development

To build the Windows core, install Visual Studio C++ x64 tools and WinDivert 2.2.2-A on Windows 10/11 x64, then run:

```powershell
cd Windows
.\compile.ps1 -Compiler msvc -NoSign
```

KokoroBox builds the macOS extension from `MacOS/ProxyBridge/ProxyBridge.xcodeproj` using `MacOS/ProxyBridge/kokorobox-ext.xcconfig`. Its release pipeline supplies the architecture and signing settings.

## Documentation

- [KokoroBox integration contract](KOKOROBOX.md)
- Component guides: [Windows](Windows/README.md), [macOS](MacOS/README.md)
- [Security policy](SECURITY.md)

## License

ProxyBridge is licensed under the [MIT License](LICENSE), with the original copyright and attribution retained. The Windows implementation uses [WinDivert](https://reqrypt.org/windivert.html); its license and redistribution requirements also apply.
