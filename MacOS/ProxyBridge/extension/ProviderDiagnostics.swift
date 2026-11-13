import Foundation
import NetworkExtension
import os.log

// Lifecycle messages can be read in Console after the extension exits.
enum ProviderDiagnostics {
    private static let osLog = OSLog(subsystem: "com.amamiyakokoro.app.proxy-extension", category: "provider")

    static func note(_ message: String, error: Bool = false) {
        os_log("%{public}@", log: osLog, type: error ? .error : .default, message)
    }

    // Include the numeric value for stop reasons added by newer versions of macOS.
    private static let reasons: [Int: String] = [
        0: "none",
        1: "stopped by the user",
        2: "the provider failed",
        3: "no network available",
        4: "the network changed and could not be recovered",
        5: "the provider was disabled",
        6: "authentication was cancelled",
        7: "the configuration failed",
        8: "idle timeout",
        9: "the configuration was disabled",
        10: "the configuration was removed",
        11: "replaced by another configuration",
        12: "the user logged out",
        13: "the user was switched",
        14: "the connection failed",
        15: "the Mac went to sleep",
        16: "the app was updated",
        17: "internal error",
    ]

    static func text(for reason: NEProviderStopReason) -> String {
        let raw = reason.rawValue
        return "\(reasons[raw] ?? "unknown reason") (\(raw))"
    }
}
