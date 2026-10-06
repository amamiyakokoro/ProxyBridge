# KokoroBox integration

KokoroBox Desktop pins this independently maintained repository at an exact commit and uses its Windows routing core and macOS system extension. The original copyright and [MIT license](LICENSE) remain unchanged.

## Windows

KokoroBox builds `ProxyBridgeCore.dll` for Windows x64. The core includes these integration changes:

- A `PROXY` rule resolves to `BLOCK` when its proxy configuration is missing, invalid, or incompatible with UDP. It never falls back to `DIRECT`.
- Managed routing explicitly bypasses IPv4, IPv6, and IPv4-mapped loopback traffic before owner lookup. This keeps ownerless local replies from delaying other packets; external traffic retains fail-closed handling. The bypass is opt-in and does not skip relay translation.
- An explicit API controls whether an application's UDP/53 DNS queries participate in `PROXY` rules, including TCP-only rules. Other UDP traffic is unaffected.
- `MAX_PROCESS_NAME` is 65536 bytes, allowing the controlled router to guard a bounded list of canonical executable paths during atomic rule replacement.

KokoroBox's separate `kokorobox-process-router.exe` supplies the fixed local SOCKS endpoint, validates a versioned command protocol, and installs mandatory rules to prevent routing loops.
The Windows core rejects proxy credentials because SOCKS5 and HTTP proxy authentication would send them in cleartext.

## macOS

KokoroBox uses the `NETransparentProxyProvider` system extension. Its controlled mode has these boundaries:

- A KokoroBox-specific provider message atomically replaces typed signing-identifier or process-name rules for the fixed `127.0.0.1:7891` SOCKS5 endpoint.
- Signing identifiers come from `NEFlowMetaData`. Process names are resolved from the source audit token and executable path.
- If the proxy service is unavailable, a `PROXY` decision resolves to `BLOCK`.
- KokoroBox, its helper and extension, Mihomo, loopback, link-local, multicast, and broadcast traffic are excluded to prevent routing loops.

This repository does not include the standalone ProxyBridge GUI, updater, or macOS DNS proxy provider. See the [Windows](Windows/README.md) and [macOS](MacOS/README.md) component guides for build details.

## macOS routing logs

The provider keeps activity and connection entries in a 500-entry in-memory ring.
`getLogs` drains up to 100 entries and returns a JSON array (including an empty
array when there are no entries). `clearLogs` atomically empties the ring and
acknowledges with `{ "status": "ok" }`. These actions use the existing
NetworkExtension provider-message channel.

KokoroBox TCP decisions record the application, destination, port and
Proxy/Direct/Block result. UDP proxy records retain their existing per-destination
deduplication; UDP Direct/Block decisions have unknown destinations because those
flows are not opened for inspection. Connection records include an ISO 8601
`timestamp` and obey `diagnosticLogging`. Activity/error records remain available
independently of the traffic switch. Excluded local-network traffic is not logged.
Logs are not persisted and disappear when the provider process exits.

Run `sh MacOS/tests/routing-logs.sh` on macOS to check ring capacity, batched reads,
clear, connection timestamps and concurrent access without activating an extension.
