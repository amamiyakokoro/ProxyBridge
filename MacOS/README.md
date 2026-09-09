# macOS system extension

KokoroBox uses only the `NETransparentProxyProvider` system extension. The
standalone ProxyBridge SwiftUI application and DNS proxy provider are not part
of this repository.

KokoroBox's build pipeline invokes the `extension` scheme with
`MacOS/ProxyBridge/kokorobox-ext.xcconfig`, which provides its signing identity,
bundle identifier, and target architecture. The resulting product is
`KokoroBoxProxyExtension.systemextension`.
