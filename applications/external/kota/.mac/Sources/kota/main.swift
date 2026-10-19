// kota: pushes Claude plan usage to the Kota app on a Flipper Zero over Bluetooth LE.
//
// Runs as a background agent (`brew services start kota`). Finds the Flipper,
// waits for the Kota app's "KOTA" hello, then sends one line every two minutes:
//   U <session%> <session_reset_s> <week%> <week_reset_s>\n

import CoreBluetooth
import Foundation

let version = "0.2.0"
let txChar = CBUUID(string: "19ED82AE-ED21-4C9D-4145-228E61FE0000") // Flipper -> host (notify)
let rxChar = CBUUID(string: "19ED82AE-ED21-4C9D-4145-228E62FE0000") // host -> Flipper (write)
// Flipper advertises 0x308X, where X is the body color.
let flipperAdv = (0..<16).map { CBUUID(string: String(format: "308%X", $0)) }
let usageURL = URL(string: "https://api.anthropic.com/api/oauth/usage")!
let refreshInterval: TimeInterval = 120

func log(_ message: String) {
    let f = DateFormatter()
    f.dateFormat = "yyyy-MM-dd HH:mm:ss"
    print("\(f.string(from: Date())) \(message)")
    fflush(stdout)
}

// MARK: - Claude usage

enum KotaError: Error, CustomStringConvertible {
    case noCredentials, http(Int)
    var description: String {
        switch self {
        case .noCredentials: return "no Claude Code login found (keychain or ~/.claude/.credentials.json); run `claude` and log in"
        case .http(let code): return "usage request failed with HTTP \(code)"
        }
    }
}

/// Claude Code keeps its OAuth login in the keychain on macOS, or in a file when the keychain is
/// unavailable. Both can exist with one of them stale, so use whichever expires last.
func accessToken() throws -> String {
    var blobs: [Data] = []
    let security = Process()
    security.executableURL = URL(fileURLWithPath: "/usr/bin/security")
    security.arguments = ["find-generic-password", "-s", "Claude Code-credentials", "-w"]
    let pipe = Pipe()
    security.standardOutput = pipe
    security.standardError = FileHandle.nullDevice
    if (try? security.run()) != nil {
        let out = pipe.fileHandleForReading.readDataToEndOfFile()
        security.waitUntilExit()
        if security.terminationStatus == 0 { blobs.append(out) }
    }
    let file = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".claude/.credentials.json")
    if let data = try? Data(contentsOf: file) { blobs.append(data) }

    let logins = blobs.compactMap { blob -> (token: String, expires: Double)? in
        guard let json = try? JSONSerialization.jsonObject(with: blob) as? [String: Any],
              let oauth = json["claudeAiOauth"] as? [String: Any],
              let token = oauth["accessToken"] as? String else { return nil }
        return (token, (oauth["expiresAt"] as? Double) ?? 0)
    }
    guard let best = logins.max(by: { $0.expires < $1.expires }) else { throw KotaError.noCredentials }
    return best.token
}

func fetchUsage(_ done: @escaping (Result<[String: Any], Error>) -> Void) {
    let token: String
    do { token = try accessToken() } catch { return done(.failure(error)) }
    var req = URLRequest(url: usageURL, timeoutInterval: 15)
    req.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
    req.setValue("oauth-2025-04-20", forHTTPHeaderField: "anthropic-beta")
    URLSession.shared.dataTask(with: req) { data, resp, err in
        let result: Result<[String: Any], Error>
        if let err {
            result = .failure(err)
        } else if let code = (resp as? HTTPURLResponse)?.statusCode, code != 200 {
            result = .failure(KotaError.http(code))
        } else if let data, let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
            result = .success(json)
        } else {
            result = .failure(KotaError.http(0))
        }
        DispatchQueue.main.async { done(result) }
    }.resume()
}

func parseDate(_ s: String) -> Date? {
    let f = ISO8601DateFormatter()
    f.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    if let d = f.date(from: s) { return d }
    f.formatOptions = [.withInternetDateTime]
    return f.date(from: s)
}

func buildLine(_ usage: [String: Any], now: Date) -> String {
    func window(_ key: String) -> (Int, Int) {
        guard let w = usage[key] as? [String: Any] else { return (0, 0) }
        let pct = Int(((w["utilization"] as? Double) ?? 0).rounded())
        var left = 0
        if let s = w["resets_at"] as? String, let d = parseDate(s) {
            left = max(0, Int(d.timeIntervalSince(now)))
        }
        return (pct, left)
    }
    let s = window("five_hour"), w = window("seven_day")
    return "U \(s.0) \(s.1) \(w.0) \(w.1)\n"
}

// MARK: - Bluetooth

final class Kota: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rx: CBCharacteristic?
    private var linked = false
    private var usage: [String: Any]?
    private var fetchedAt = Date.distantPast
    private var ignoreUntil: [UUID: Date] = [:]
    private var helloTimer: Timer?
    private var refreshTimer: Timer?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        switch c.state {
        case .poweredOn: scan()
        case .unauthorized: log("Bluetooth permission denied: allow kota in System Settings > Privacy & Security > Bluetooth")
        case .poweredOff: log("Bluetooth is off")
        default: break
        }
    }

    private func scan() {
        guard central.state == .poweredOn, peripheral == nil, !central.isScanning else { return }
        log("looking for the Flipper (open Kota on it)")
        // Duplicates on, so a Flipper we skipped comes back once its back-off ends.
        central.scanForPeripherals(withServices: flipperAdv,
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral,
                        advertisementData: [String: Any], rssi: NSNumber) {
        if let until = ignoreUntil[p.identifier], until > Date() { return }
        c.stopScan()
        peripheral = p
        p.delegate = self
        c.connect(p)
    }

    func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
        p.discoverServices(nil)
    }

    func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        log("connect failed: \(error?.localizedDescription ?? "unknown")")
        reset()
    }

    func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
        if linked { log("disconnected") }
        reset()
    }

    private func reset() {
        helloTimer?.invalidate()
        refreshTimer?.invalidate()
        peripheral = nil
        rx = nil
        linked = false
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { self.scan() }
    }

    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        for s in p.services ?? [] { p.discoverCharacteristics([txChar, rxChar], for: s) }
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        guard let chars = s.characteristics, chars.contains(where: { $0.uuid == txChar }) else { return }
        for ch in chars {
            if ch.uuid == rxChar { rx = ch }
            if ch.uuid == txChar { p.setNotifyValue(true, for: ch) }
        }
        // Kota says hello every second; the stock firmware profile never does.
        helloTimer = Timer.scheduledTimer(withTimeInterval: 5, repeats: false) { [weak self] _ in
            guard let self, !self.linked, let p = self.peripheral else { return }
            log("Flipper found but Kota is not open on it")
            self.ignoreUntil[p.identifier] = Date().addingTimeInterval(30)
            self.central.cancelPeripheralConnection(p)
        }
    }

    func peripheral(_ p: CBPeripheral, didUpdateValueFor ch: CBCharacteristic, error: Error?) {
        guard ch.uuid == txChar, let value = ch.value else { return }
        let lines = String(decoding: value, as: UTF8.self).split(separator: "\n")
        if linked, lines.contains("R") {
            // OK pressed on the Flipper. Don't hammer the API if it is pressed repeatedly.
            if Date().timeIntervalSince(fetchedAt) > 10 { fetchedAt = .distantPast }
            log("refresh requested")
            return tick()
        }
        guard !linked, lines.contains(where: { $0.hasPrefix("KOTA") }) else { return }
        linked = true
        helloTimer?.invalidate()
        log("Kota connected (\(p.name ?? "Flipper"))")
        tick()
        refreshTimer = Timer.scheduledTimer(withTimeInterval: refreshInterval, repeats: true) { [weak self] _ in
            self?.tick()
        }
    }

    private func tick() {
        guard Date().timeIntervalSince(fetchedAt) >= refreshInterval - 5 else { return send() }
        fetchedAt = Date()
        fetchUsage { [weak self] result in
            switch result {
            case .success(let u): self?.usage = u
            case .failure(let e): log("usage fetch failed: \(e)") // keep showing the last good numbers
            }
            self?.send()
        }
    }

    private func send() {
        guard linked, let p = peripheral, let rx, let usage else { return }
        let line = buildLine(usage, now: Date())
        p.writeValue(Data(line.utf8), for: rx, type: .withResponse)
        log("sent \(line.trimmingCharacters(in: .newlines))")
    }
}

// MARK: - main

func selfCheck() {
    let now = Date(timeIntervalSince1970: 1_000_000)
    let usage: [String: Any] = [
        "five_hour": ["utilization": 33.6, "resets_at": "1970-01-12T14:46:40.279446+00:00"],
        "seven_day": NSNull(),
    ]
    let line = buildLine(usage, now: now)
    precondition(line == "U 34 3600 0 0\n", "buildLine: \(line)")
    print("ok")
}

switch CommandLine.arguments.dropFirst().first {
case "--self-check":
    selfCheck()
case "--version":
    print("kota \(version)")
case "--once":
    // Print the line that would be sent, without Bluetooth. Handy for checking the login.
    fetchUsage { result in
        switch result {
        case .success(let u): print(buildLine(u, now: Date()), terminator: "")
        case .failure(let e): print("error: \(e)"); exit(1)
        }
        exit(0)
    }
    RunLoop.main.run()
default:
    log("kota \(version)")
    let kota = Kota()
    withExtendedLifetime(kota) { RunLoop.main.run() }
}
