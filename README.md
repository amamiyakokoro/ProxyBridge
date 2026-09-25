<div align="center">

# ProxyBridge for KokoroBox

Application-routing components for [KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop).

[Security](SECURITY.md) · [License](LICENSE)

</div>

## Features

- Per-application proxy routing on Windows and macOS through KokoroBox's local Mihomo SOCKS5 listener
- `PROXY` rules block traffic when the proxy is unavailable

## Supported platforms

Windows 10/11 x64 (`ProxyBridgeCore.dll`) and macOS 13+ (`NETransparentProxyProvider` system extension). This repository is used by KokoroBox and does not provide a standalone app.

## Get started

Download [KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop/releases) to use application routing.

## Development

See the [Windows](Windows/README.md) and [macOS](MacOS/README.md) component guides for build details.

## Documentation

- [KokoroBox integration details](KOKOROBOX.md)
- [Security policy](SECURITY.md)

## License

ProxyBridge is licensed under the [MIT License](LICENSE). The Windows component uses [WinDivert](https://reqrypt.org/windivert.html).
