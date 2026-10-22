import Foundation
import NetworkExtension

// System-owned providers and flows cannot be initialized/destroyed outside an
// activated extension. Shadow their transport types and substitute the provider
// superclass. All production method bodies are compiled unchanged; Xcode builds
// separately verify the real framework signatures.
class NEAppProxyFlow: NSObject {
    var metaData: NEFlowMetaData { fatalError() }
    func open(withLocalEndpoint endpoint: NWHostEndpoint?, completionHandler: @escaping (Error?) -> Void) { fatalError() }
    func closeReadWithError(_ error: Error?) { fatalError() }
    func closeWriteWithError(_ error: Error?) { fatalError() }
}

class NEAppProxyUDPFlow: NEAppProxyFlow {
    func readDatagrams(completionHandler: @escaping ([Data]?, [NWEndpoint]?, Error?) -> Void) { fatalError() }
    func writeDatagrams(_ data: [Data], sentBy endpoints: [NWEndpoint], completionHandler: @escaping (Error?) -> Void) { fatalError() }
}

class NEAppProxyTCPFlow: NEAppProxyFlow {
    var remoteEndpoint: NWEndpoint { fatalError() }
    func readData(completionHandler: @escaping (Data?, Error?) -> Void) { fatalError() }
    func write(_ data: Data, withCompletionHandler completionHandler: @escaping (Error?) -> Void) { fatalError() }
}

class NWTCPConnection: NSObject {
    func readMinimumLength(_ minimum: Int, maximumLength maximum: Int,
                           completionHandler: @escaping (Data?, Error?) -> Void) { fatalError() }
    func write(_ data: Data, completionHandler: @escaping (Error?) -> Void) { fatalError() }
    func cancel() { fatalError() }
}

class NWUDPSession: NSObject {
    func setReadHandler(_ handler: @escaping ([Data]?, Error?) -> Void, maxDatagrams: Int) { fatalError() }
    func writeMultipleDatagrams(_ data: [Data], completionHandler: @escaping (Error?) -> Void) { fatalError() }
    func cancel() { fatalError() }
}

class RegressionProxyProvider: NSObject {
    var protocolConfiguration: NEVPNProtocol?
    var udpSessionFactory: ((NWEndpoint) -> NWUDPSession)?

    func startProxy(options: [String: Any]?, completionHandler: @escaping (Error?) -> Void) { fatalError() }
    func stopProxy(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) { fatalError() }
    func handleAppMessage(_ data: Data, completionHandler: ((Data?) -> Void)?) { fatalError() }
    func handleNewFlow(_ flow: NEAppProxyFlow) -> Bool { fatalError() }
    func sleep(completionHandler: @escaping () -> Void) { fatalError() }
    func wake() { fatalError() }
    func setTunnelNetworkSettings(_ settings: NETunnelNetworkSettings?, completionHandler: @escaping (Error?) -> Void) { fatalError() }
    func createTCPConnection(to endpoint: NWEndpoint, enableTLS: Bool,
                             tlsParameters: NWTLSParameters?, delegate: Any?) -> NWTCPConnection { fatalError() }
    func createUDPSession(to endpoint: NWEndpoint, from local: NWHostEndpoint?) -> NWUDPSession {
        guard let udpSessionFactory else { fatalError("Unexpected UDP session creation") }
        return udpSessionFactory(endpoint)
    }
}
