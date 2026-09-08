# ProxyBridge macOS provider for KokoroBox

This fork supplies the `NETransparentProxyProvider` system extension used by
[KokoroBox Desktop](https://github.com/amamiyakokoro/KokoroBox-Desktop) for
application routing on macOS 13 and later. It is not a standalone macOS
ProxyBridge release: the upstream SwiftUI app and DNS proxy provider are not
packaged by KokoroBox.

## Runtime

KokoroBox atomically installs a bounded, signing-identifier policy in the
provider. Matching traffic can be proxied, sent direct, or blocked.

- Proxy traffic uses the fixed local Mihomo SOCKS5 endpoint at
  `127.0.0.1:7891`.
- An unavailable proxy turns a `PROXY` decision into `BLOCK`; it never falls
  back to direct traffic.
- KokoroBox, its helper and extension, Mihomo, loopback, link-local, multicast,
  and broadcast traffic remain direct to prevent routing loops.
- When enabled by KokoroBox, protected DNS is relayed through Mihomo's local
  DNS listener rather than sent to its original destination.

See [KOKOROBOX.md](../KOKOROBOX.md) for the integration contract and KokoroBox
Desktop's macOS application-routing documentation for packaging and activation.

## Build

The extension target is in `ProxyBridge.xcodeproj`. KokoroBox builds it with
`kokorobox-ext.xcconfig`, supplies `KOKOROBOX_TARGET_ARCH`, then signs and
embeds the resulting system extension in the KokoroBox app. A distributable
build requires macOS 13+, Xcode, matching provisioning profiles, and Network
Extension entitlements.

The configuration registers only the transparent app-proxy provider. It does
not enable the upstream DNS proxy provider.

## Upstream and license

For standalone ProxyBridge documentation, use
[README.upstream.md](../README.upstream.md). This fork retains the upstream
[MIT License](../LICENSE) and attribution.
