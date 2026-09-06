# KokoroBox integration changes

This fork is the pinned ProxyBridge source used by KokoroBox Desktop's Windows x64 application
routing feature. The upstream copyright and MIT license remain unchanged.

The KokoroBox integration carries two narrowly scoped Windows core changes:

- A rule whose action is `PROXY` resolves to `BLOCK` when its proxy configuration is missing,
  invalid, or incompatible with UDP. It never falls back to `DIRECT`.
- An explicit API controls whether an application's own UDP/53 DNS queries participate in
  `PROXY` rules, including TCP-only rules. It does not widen any other UDP traffic.
- `MAX_PROCESS_NAME` is increased to 65536 bytes so the controlled router can atomically guard
  a bounded list of canonical executable paths while replacing rules.

KokoroBox builds `ProxyBridgeCore.dll` from this fork at an exact commit. Its separate
`kokorobox-process-router.exe` wrapper supplies the fixed local SOCKS endpoint, validates a
versioned command protocol, installs mandatory loop-prevention rules, and does not include the
ProxyBridge GUI or updater.
