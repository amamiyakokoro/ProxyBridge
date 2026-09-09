# KokoroBox integration changes

This independently maintained repository is the pinned ProxyBridge source used by KokoroBox
Desktop's Windows x64 application-routing feature. The original copyright and MIT license remain
unchanged.

The KokoroBox integration carries three narrowly scoped Windows core changes:

- A rule whose action is `PROXY` resolves to `BLOCK` when its proxy configuration is missing,
  invalid, or incompatible with UDP. It never falls back to `DIRECT`.
- An explicit API controls whether an application's own UDP/53 DNS queries participate in
  `PROXY` rules, including TCP-only rules. It does not widen any other UDP traffic.
- `MAX_PROCESS_NAME` is increased to 65536 bytes so the controlled router can atomically guard
  a bounded list of canonical executable paths while replacing rules.

KokoroBox builds `ProxyBridgeCore.dll` from this repository at an exact commit. Its separate
`kokorobox-process-router.exe` wrapper supplies the fixed local SOCKS endpoint, validates a
versioned command protocol, installs mandatory loop-prevention rules, and does not include the
ProxyBridge GUI or updater.

The macOS integration reuses only the `NETransparentProxyProvider` system extension. A
KokoroBox-specific provider message atomically replaces signing-identifier rules and the fixed
`127.0.0.1:7891` SOCKS5 endpoint. In this controlled mode, unavailable proxy service resolves a
`PROXY` decision to `BLOCK`. KokoroBox, its helper and extension, Mihomo, loopback, link-local,
multicast, and broadcast traffic are permanently excluded to prevent routing loops. The
standalone SwiftUI GUI and DNS proxy provider are not embedded.
