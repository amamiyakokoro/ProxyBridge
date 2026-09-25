# KokoroBox integration

KokoroBox Desktop pins this independently maintained repository at an exact commit and uses its Windows routing core and macOS system extension. The original copyright and [MIT license](LICENSE) remain unchanged.

## Windows

KokoroBox builds `ProxyBridgeCore.dll` for Windows x64. The core includes three integration changes:

- A `PROXY` rule resolves to `BLOCK` when its proxy configuration is missing, invalid, or incompatible with UDP. It never falls back to `DIRECT`.
- An explicit API controls whether an application's UDP/53 DNS queries participate in `PROXY` rules, including TCP-only rules. Other UDP traffic is unaffected.
- `MAX_PROCESS_NAME` is 65536 bytes, allowing the controlled router to guard a bounded list of canonical executable paths during atomic rule replacement.

KokoroBox's separate `kokorobox-process-router.exe` supplies the fixed local SOCKS endpoint, validates a versioned command protocol, and installs mandatory rules to prevent routing loops.

## macOS

KokoroBox uses the `NETransparentProxyProvider` system extension. Its controlled mode has these boundaries:

- A KokoroBox-specific provider message atomically replaces typed signing-identifier or process-name rules for the fixed `127.0.0.1:7891` SOCKS5 endpoint.
- Signing identifiers come from `NEFlowMetaData`. Process names are resolved from the source audit token and executable path.
- If the proxy service is unavailable, a `PROXY` decision resolves to `BLOCK`.
- KokoroBox, its helper and extension, Mihomo, loopback, link-local, multicast, and broadcast traffic are excluded to prevent routing loops.

This repository does not include the standalone ProxyBridge GUI, updater, or macOS DNS proxy provider. See the [Windows](Windows/README.md) and [macOS](MacOS/README.md) component guides for build details.
