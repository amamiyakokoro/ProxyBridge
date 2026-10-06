// Compiled beside the provider's private buffer. No System Extension activation.
private let buffer = RoutingLogBuffer()
for index in 0..<510 {
    buffer.append(.activity(timestamp: "now", level: "INFO", message: "entry-\(index)"))
}
let batch = buffer.drain()
precondition(batch.count == 100 && batch.first?["message"] == "entry-10")
precondition(batch.last?["message"] == "entry-109")
buffer.clear()
precondition(buffer.drain().isEmpty)
let timestamp = ISO8601DateFormatter().string(from: Date())
buffer.append(.connection(timestamp: timestamp, proto: "TCP", process: "curl", destination: "example.com", port: "443", proxy: "BLOCK"))
buffer.append(.connection(timestamp: timestamp, proto: "UDP", process: "curl", destination: "1.1.1.1", port: "53", proxy: "SOCKS5"))
let connections = buffer.drain()
precondition(connections.count == 2)
precondition(connections[0]["proxy"] == "BLOCK" && connections[1]["protocol"] == "UDP")
precondition(ISO8601DateFormatter().date(from: connections[0]["timestamp"]!) != nil)
precondition(buffer.drain().isEmpty)
// Simultaneous writes and clears must leave valid indices and bounded batches.
DispatchQueue.concurrentPerform(iterations: 1000) { index in
    if index % 7 == 0 { buffer.clear() }
    else { buffer.append(.activity(timestamp: "now", level: "INFO", message: "concurrent")) }
    if index % 3 == 0 { precondition(buffer.drain().count <= 100) }
}
buffer.clear()
precondition(buffer.drain().isEmpty)
print("Provider logs: bounded capacity, draining, clear, timestamp and concurrency checks passed")
