# Security policy

This repository provides privileged networking components used only by
KokoroBox Desktop:

- Windows x64 `ProxyBridgeCore.dll`, which uses the WinDivert driver.
- The macOS `NETransparentProxyProvider` system extension.

The repository does not distribute a standalone application, updater, GUI,
CLI, installer, or Linux implementation.

## Reporting a vulnerability

Do not open a public issue for a suspected vulnerability. Use the repository's
private GitHub security advisory reporting flow and include the affected
component, supported platform, reproduction steps, and an assessment of the
impact on KokoroBox users.

## Supported code

Only the latest `master` revision is supported. KokoroBox pins an exact
revision and updates its package after validation; users receive fixes through
KokoroBox releases rather than a separate ProxyBridge updater.

## Security boundaries

Windows interception is performed by WinDivert and requires administrator
authorization. The core's local relay listeners are intended for the
KokoroBox-owned process router and must not be exposed beyond localhost.

On macOS, the extension accepts only KokoroBox's versioned policy protocol,
uses the fixed local SOCKS endpoint, and turns unavailable proxy service into
`BLOCK` rather than `DIRECT`. Its app group and Network Extension entitlement
are limited to KokoroBox's identifiers.
