# macOS system extension

KokoroBox uses only the `NETransparentProxyProvider` system extension. The
standalone ProxyBridge SwiftUI application and DNS proxy provider are not part
of this repository.

KokoroBox's build pipeline invokes the `extension` scheme with
`MacOS/ProxyBridge/kokorobox-ext.xcconfig`, which provides its signing identity,
bundle identifier, and target architecture. The resulting product is
`KokoroBoxProxyExtension.systemextension`.

Run `sh MacOS/tests/provider-regressions.sh` to check UDP association release,
Mihomo DNS cleanup, reused-flow callbacks, SOCKS5 reply framing, HTTP CONNECT
headers and tunnel payload ordering. The tests compile the real provider method
bodies with transport stand-ins that retain callbacks after cancellation; no
extension activation or network access is required. The unsigned Xcode build
separately checks compatibility with the actual NetworkExtension APIs.
