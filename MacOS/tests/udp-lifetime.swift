// Appended to the real provider source by provider-regressions.sh. These stand-ins
// deliberately retain callbacks after cancel/close, like NetworkExtension can.
private final class LifetimeUDPFlow: NEAppProxyUDPFlow {
    var pendingRead: (([Data]?, [NWEndpoint]?, Error?) -> Void)?
    var readCloses = 0
    var writeCloses = 0

    override var hash: Int { ObjectIdentifier(self).hashValue }
    override func isEqual(_ object: Any?) -> Bool {
        guard let other = object as? LifetimeUDPFlow else { return false }
        return self === other
    }

    override func readDatagrams(completionHandler: @escaping ([Data]?, [NWEndpoint]?, Error?) -> Void) {
        pendingRead = completionHandler
    }
    override func closeReadWithError(_ error: Error?) { readCloses += 1 }
    override func closeWriteWithError(_ error: Error?) { writeCloses += 1 }
}

private final class LifetimeUDPSession: NWUDPSession {
    var handler: (([Data]?, Error?) -> Void)?
    var handlerInstalls = 0
    var cancellations = 0

    override func setReadHandler(_ handler: @escaping ([Data]?, Error?) -> Void, maxDatagrams: Int) {
        self.handler = handler
        handlerInstalls += 1
    }
    override func cancel() { cancellations += 1 }
}

private final class LifetimeTCPConnection: NWTCPConnection {
    var pendingRead: ((Data?, Error?) -> Void)?
    var cancellations = 0

    override func readMinimumLength(_ minimum: Int, maximumLength maximum: Int,
                                    completionHandler: @escaping (Data?, Error?) -> Void) {
        pendingRead = completionHandler
    }
    override func cancel() { cancellations += 1 }
}

extension AppProxyProvider {
    static func testUDPLifetimes() {
        // Exercise ordinary teardown, control EOF and provider stop, with both
        // SOCKS5 and Mihomo DNS handlers installed on the real association.
        for ending in ["teardown", "control EOF", "stop"] {
            let provider = AppProxyProvider()
            let flow = LifetimeUDPFlow()
            let control = LifetimeTCPConnection()
            let relay = LifetimeUDPSession()
            let dns = LifetimeUDPSession()
            weak var retained: UDPAssociation?
            do {
                let association = UDPAssociation(clientFlow: flow, controlConnection: control,
                    udpSession: relay, dnsSession: dns, displayName: "test")
                retained = association
                provider.udpAssociations[flow] = association
                provider.readAndForwardClientUDP(association)
                provider.readAndForwardRelayUDP(association)
                provider.readAndForwardDnsUDP(association)
                provider.monitorControlConnection(association)
            }
            // A callback already queued by NetworkExtension can outlive handler
            // replacement. It must hold neither the association nor its flow.
            let oldRelayHandler = relay.handler!
            let oldDNSHandler = dns.handler!
            switch ending {
            case "control EOF": control.pendingRead?(nil, nil)
            case "stop": provider.stopProxy(with: .userInitiated) {}
            default: provider.teardownUDP(flow)
            }
            precondition(retained == nil, "Association leaked after \(ending)")
            precondition(relay.handlerInstalls == 2 && dns.handlerInstalls == 2,
                         "Both session handlers must be detached before cancellation")
            precondition(control.cancellations == 1 && relay.cancellations == 1 && dns.cancellations == 1)
            precondition(flow.readCloses == 1 && flow.writeCloses == 1)
            oldRelayHandler(nil, NSError(domain: "test", code: 1))
            oldDNSHandler(nil, NSError(domain: "test", code: 1))
            flow.pendingRead?(nil, nil, nil)
            provider.teardownUDP(flow)
            precondition(flow.readCloses == 1 && control.cancellations == 1,
                         "Late callbacks must not repeat teardown")
        }

        let provider = AppProxyProvider()
        let flow = LifetimeUDPFlow()
        let oldControl = LifetimeTCPConnection()
        let oldRelay = LifetimeUDPSession()
        let oldDNS = LifetimeUDPSession()
        weak var oldAssociation: UDPAssociation?
        do {
            let association = UDPAssociation(clientFlow: flow, controlConnection: oldControl,
                udpSession: oldRelay, dnsSession: oldDNS, displayName: "old")
            oldAssociation = association
            provider.udpAssociations[flow] = association
            provider.readAndForwardClientUDP(association)
            provider.readAndForwardRelayUDP(association)
            provider.readAndForwardDnsUDP(association)
            provider.monitorControlConnection(association)
        }
        let oldRead = flow.pendingRead!
        let oldRelayHandler = oldRelay.handler!
        let oldDNSHandler = oldDNS.handler!
        let newControl = LifetimeTCPConnection()
        let newRelay = LifetimeUDPSession()
        provider.udpSessionFactory = { _ in newRelay }
        provider.pendingHandshakes[ObjectIdentifier(newControl)] = PendingHandshake(
            token: UUID(), flow: flow, connection: newControl)
        provider.relayUDPThroughSOCKS5(clientFlow: flow, relayHost: "127.0.0.1",
            relayPort: 7891, tcpConnection: newControl, displayName: "new")
        precondition(oldAssociation == nil, "Replacement retained the old association")
        precondition(oldRelay.handlerInstalls == 2 && oldDNS.handlerInstalls == 2)
        precondition(oldControl.cancellations == 1 && oldRelay.cancellations == 1 && oldDNS.cancellations == 1)
        oldRead(nil, nil, nil)
        oldRelayHandler(nil, NSError(domain: "test", code: 1))
        oldDNSHandler(nil, NSError(domain: "test", code: 1))
        oldControl.pendingRead?(nil, nil)
        precondition(provider.udpAssociations[flow]?.controlConnection === newControl)
        precondition(newControl.cancellations == 0 && flow.readCloses == 0 && flow.writeCloses == 0,
                     "Old callbacks must not close a reused flow or its new association")
        provider.teardownUDP(flow)

        // A pending client read must not keep the flow alive after association
        // removal, even if the flow keeps its callback after close.
        weak var releasedFlow: LifetimeUDPFlow?
        autoreleasepool {
            let transient = LifetimeUDPFlow()
            releasedFlow = transient
            do {
                let association = UDPAssociation(clientFlow: transient, controlConnection: LifetimeTCPConnection(),
                    udpSession: LifetimeUDPSession(), dnsSession: nil, displayName: "transient")
                provider.udpAssociations[transient] = association
                provider.readAndForwardClientUDP(association)
                provider.readAndForwardRelayUDP(association)
            }
            provider.teardownUDP(transient)
        }
        precondition(releasedFlow == nil, "Pending client read retained the closed flow")
        print("UDP lifetime: teardown, EOF, stop, DNS, replacement and queued callbacks passed")
    }
}

AppProxyProvider.testUDPLifetimes()
