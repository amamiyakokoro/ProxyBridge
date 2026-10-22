private final class ReplyTCPConnection: NWTCPConnection {
    enum Event { case bytes(Data), failure(Error), eof, pending }
    var events: [Event]
    var reads: [(minimum: Int, maximum: Int)] = []
    var pendingRead: ((Data?, Error?) -> Void)?
    var cancellations = 0
    var bytesRead = 0

    init(_ events: [Event]) { self.events = events }
    var remainingBytes: Data {
        events.reduce(into: Data()) { if case .bytes(let data) = $1 { $0.append(data) } }
    }
    override func readMinimumLength(_ minimum: Int, maximumLength maximum: Int,
                                    completionHandler: @escaping (Data?, Error?) -> Void) {
        reads.append((minimum, maximum))
        precondition(minimum > 0 && maximum >= minimum)
        var received = Data()
        while received.count < minimum && !events.isEmpty {
            switch events.removeFirst() {
            case .bytes(let data):
                let count = min(data.count, maximum - received.count)
                received.append(data.prefix(count))
                bytesRead += count
                if count < data.count { events.insert(.bytes(Data(data.dropFirst(count))), at: 0) }
            case .failure(let error): completionHandler(nil, error); return
            case .eof: completionHandler(received.isEmpty ? nil : received, nil); return
            case .pending:
                precondition(received.isEmpty)
                pendingRead = completionHandler
                return
            }
        }
        completionHandler(received.isEmpty ? nil : received, nil)
    }
    override func write(_ data: Data, completionHandler: @escaping (Error?) -> Void) { completionHandler(nil) }
    override func cancel() { cancellations += 1 }
}

private final class ReplyTCPFlow: NEAppProxyTCPFlow {
    var opens = 0
    var writes: [Data] = []
    var readCloses = 0
    var writeCloses = 0
    var deferWrites = false
    var pendingWrites: [(Error?) -> Void] = []
    var pendingRead: ((Data?, Error?) -> Void)?

    override func open(withLocalEndpoint endpoint: NWHostEndpoint?, completionHandler: @escaping (Error?) -> Void) {
        opens += 1
        completionHandler(nil)
    }
    override func readData(completionHandler: @escaping (Data?, Error?) -> Void) { pendingRead = completionHandler }
    override func write(_ data: Data, withCompletionHandler completionHandler: @escaping (Error?) -> Void) {
        writes.append(data)
        if deferWrites { pendingWrites.append(completionHandler) } else { completionHandler(nil) }
    }
    override func closeReadWithError(_ error: Error?) { readCloses += 1; pendingRead = nil }
    override func closeWriteWithError(_ error: Error?) { writeCloses += 1 }
}

private final class ReplyUDPFlow: NEAppProxyUDPFlow {
    override func readDatagrams(completionHandler: @escaping ([Data]?, [NWEndpoint]?, Error?) -> Void) {}
    override func closeReadWithError(_ error: Error?) {}
    override func closeWriteWithError(_ error: Error?) {}
}

private final class ReplyUDPSession: NWUDPSession {
    override func setReadHandler(_ handler: @escaping ([Data]?, Error?) -> Void, maxDatagrams: Int) {}
    override func cancel() {}
}

extension AppProxyProvider {
    static func testProxyReplies() {
        let provider = AppProxyProvider()
        let banner = Data([0, 255, 13, 10, 65, 66])
        let ipv4 = Data([5, 0, 0, 1, 127, 0, 0, 1, 31, 144])
        let ipv6 = Data([5, 0, 0, 4, 0x20, 1, 0x0d, 0xb8] + [UInt8](repeating: 0, count: 11) + [1, 31, 144])
        let shortDomain = Data([5, 0, 0, 3, 1, 97, 31, 144])
        let longDomain = Data([5, 0, 0, 3, 255] + [UInt8](repeating: 97, count: 255) + [31, 144])

        func socks(_ events: [ReplyTCPConnection.Event]) -> (SOCKS5Reply, ReplyTCPConnection) {
            let connection = ReplyTCPConnection(events)
            var result: SOCKS5Reply?
            var completions = 0
            provider.readSOCKS5Reply(connection) { result = $0; completions += 1 }
            precondition(completions == 1)
            precondition(connection.reads.allSatisfy { $0.minimum == $0.maximum }, "Handshake read could swallow tunnel data")
            return (result!, connection)
        }
        for reply in [ipv4, ipv6, shortDomain, longDomain] {
            let layouts: [[ReplyTCPConnection.Event]] = [
                [.bytes(reply + banner)],
                reply.map { .bytes(Data([$0])) } + [.bytes(banner)]
            ]
            for layout in layouts {
                let (result, connection) = socks(layout)
                guard case .success(let parsed) = result else { fatalError("Valid SOCKS5 reply rejected") }
                precondition(parsed == reply && connection.remainingBytes == banner)
            }
        }
        let malformed: [Data] = [Data(), Data([5]), Data([4, 0]), Data([5, 0, 1, 1]),
            Data([5, 0, 0, 9]), Data([5, 0, 0, 3, 0]), Data(ipv4.dropLast()), Data(ipv6.prefix(10)),
            Data([5, 0, 0, 3, 3, 97])]
        for reply in malformed {
            guard case .failed = socks([.bytes(reply), .eof]).0 else { fatalError("Malformed SOCKS5 reply accepted") }
        }
        guard case .failed = socks([.failure(NSError(domain: "test", code: 1))]).0 else { fatalError() }
        guard case .rejected(5) = socks([.bytes(Data([5, 5]))]).0 else { fatalError("Lost short rejection reason") }

        func http(_ events: [ReplyTCPConnection.Event]) -> (Int?, Data, ReplyTCPConnection) {
            let connection = ReplyTCPConnection(events)
            var status: Int?
            var leftover = Data()
            var completions = 0
            provider.readHTTPConnectResponse(connection) { status = $0; leftover = $1; _ = $2; completions += 1 }
            precondition(completions == 1)
            return (status, leftover, connection)
        }
        let headers = Data("HTTP/1.1 200 Connection established\r\nX-Test: value\r\n\r\n".utf8)
        // Every possible two-part split includes the CRLF/CRLF boundary splits.
        for split in 0...headers.count {
            let chunks = [Data(headers.prefix(split)), Data(headers.dropFirst(split)) + banner].filter { !$0.isEmpty }
            let (status, leftover, connection) = http(chunks.map { .bytes($0) })
            precondition(status == 200 && leftover + connection.remainingBytes == banner,
                         "Fragmented HTTP reply lost binary tunnel data")
        }
        for status in [200, 201, 204, 299] {
            precondition(http([.bytes(Data("HTTP/1.0 \(status) Tunnel\r\n\r\n".utf8))]).0 == status)
        }
        let rejection = Data("HTTP/1.1 407 Proxy Authentication Required\r\nContent-Length: 200\r\n\r\n".utf8)
        precondition(http([.bytes(rejection)]).0 == 407)
        for status in ["NOTHTTP/1.1 200 OK", "HTTP/1.9 200 OK", "HTTP/1.1 2000 OK",
                       "HTTP/1.1 +200 OK", "HTTP/1.1 20x OK", "HTTP/1.1 099 Bad", "HTTP/1.1 600 Bad", ""] {
            precondition(http([.bytes(Data((status + "\r\n\r\n").utf8))]).0 == nil)
        }
        precondition(http([.bytes(Data("HTTP/1.1 200 OK\r\n".utf8)), .eof]).0 == nil)
        precondition(http([.failure(NSError(domain: "test", code: 1))]).0 == nil)
        let prefix = "HTTP/1.1 200 OK\r\nX-Pad: "
        let bounded = Data((prefix + String(repeating: "a", count: maximumHTTPConnectHeaderSize - prefix.utf8.count - 4) + "\r\n\r\n").utf8)
        let exactLimit = http([.bytes(bounded + banner)])
        precondition(exactLimit.0 == 200 && exactLimit.2.remainingBytes == banner)
        let tooLarge = http([.bytes(Data(repeating: 65, count: maximumHTTPConnectHeaderSize + 1))])
        precondition(tooLarge.0 == nil && tooLarge.2.bytesRead == maximumHTTPConnectHeaderSize)

        // Exercise the actual CONNECT entry point and relay, not only the parser.
        for reply in [ipv4, ipv6, shortDomain] {
            let connection = ReplyTCPConnection([.bytes(reply + banner), .eof])
            let flow = ReplyTCPFlow()
            provider.pendingHandshakes[ObjectIdentifier(connection)] = PendingHandshake(token: UUID(), flow: flow, connection: connection)
            provider.sendSOCKS5ConnectRequest(clientFlow: flow, proxyConnection: connection, destination: "1.2.3.4", port: 443)
            precondition(flow.opens == 1 && flow.writes.reduce(Data(), +) == banner)
        }
        // UDP ASSOCIATE uses the same exact reader. An IPv6 unspecified bind
        // address must reuse the SOCKS server host, just like 0.0.0.0.
        for reply in [shortDomain, Data([5, 0, 0, 4] + [UInt8](repeating: 0, count: 16) + [31, 144])] {
            let connection = ReplyTCPConnection([.bytes(reply), .pending])
            let flow = ReplyUDPFlow()
            var endpoint: NWHostEndpoint?
            provider.udpSessionFactory = { endpoint = $0 as? NWHostEndpoint; return ReplyUDPSession() }
            provider.pendingHandshakes[ObjectIdentifier(connection)] = PendingHandshake(token: UUID(), flow: flow, connection: connection)
            provider.sendSOCKS5UDPAssociate(clientFlow: flow, tcpConnection: connection, socksHost: "127.0.0.1", displayName: "test")
            precondition(provider.udpAssociations[flow] != nil && endpoint?.port == "8080")
            precondition(endpoint?.hostname == (reply == shortDomain ? "a" : "127.0.0.1"))
            provider.teardownUDP(flow)
        }

        let rejectedConnection = ReplyTCPConnection([.bytes(rejection)])
        let rejectedFlow = ReplyTCPFlow()
        provider.pendingHandshakes[ObjectIdentifier(rejectedConnection)] = PendingHandshake(token: UUID(), flow: rejectedFlow, connection: rejectedConnection)
        provider.handleHTTPProxy(clientFlow: rejectedFlow, proxyConnection: rejectedConnection, destination: "example.com", port: 443, username: nil, password: nil)
        precondition(rejectedFlow.opens == 0 && rejectedConnection.cancellations == 1,
                     "A 407 containing 200 must not open a tunnel")

        let nextPayload = Data("next".utf8)
        let connection = ReplyTCPConnection([.bytes(headers + banner), .bytes(nextPayload), .eof])
        let flow = ReplyTCPFlow()
        flow.deferWrites = true
        provider.pendingHandshakes[ObjectIdentifier(connection)] = PendingHandshake(token: UUID(), flow: flow, connection: connection)
        provider.handleHTTPProxy(clientFlow: flow, proxyConnection: connection, destination: "example.com", port: 443, username: nil, password: nil)
        precondition(flow.writes == [banner] && connection.reads.count == 1,
                     "Relay read started before the initial payload write completed")
        flow.pendingWrites.removeFirst()(nil)
        precondition(flow.writes == [banner, nextPayload])
        flow.pendingWrites.removeFirst()(nil)
        precondition(connection.cancellations == 1)

        let failedWriteConnection = ReplyTCPConnection([.bytes(headers + banner), .pending])
        let failedWriteFlow = ReplyTCPFlow()
        failedWriteFlow.deferWrites = true
        provider.pendingHandshakes[ObjectIdentifier(failedWriteConnection)] = PendingHandshake(
            token: UUID(), flow: failedWriteFlow, connection: failedWriteConnection)
        provider.handleHTTPProxy(clientFlow: failedWriteFlow, proxyConnection: failedWriteConnection,
            destination: "example.com", port: 443, username: nil, password: nil)
        failedWriteFlow.pendingWrites.removeFirst()(NSError(domain: "test", code: 1))
        precondition(failedWriteConnection.reads.count == 1 && failedWriteConnection.cancellations == 1)
        precondition(failedWriteFlow.readCloses == 1 && failedWriteFlow.writeCloses == 1)
        print("Proxy replies: SOCKS5 framing, UDP ASSOCIATE, HTTP headers, binary payload ordering and failures passed")
    }
}

AppProxyProvider.testProxyReplies()
