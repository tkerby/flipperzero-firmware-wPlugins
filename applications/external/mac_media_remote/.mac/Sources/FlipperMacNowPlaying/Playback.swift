import Foundation

struct Playback: Equatable {
    var source = "Mac"
    var title = ""
    var artist = ""
    var album = ""
    var playing = false
    var position: Double?
    var duration: Double?
    var artwork: Data?
    var identity: String { source + "\u{1f}" + title + "\u{1f}" + artist + "\u{1f}" + album }

    private static func sourceName(_ bundle: String?) -> String {
        switch bundle {
        case "ru.yandex.desktop.music": return "Яндекс Музыка"
        case "com.apple.Music": return "Apple Music"
        case "com.brave.Browser": return "Brave"
        case "com.google.Chrome": return "Chrome"
        case "org.mozilla.firefox": return "Firefox"
        default: return bundle?.split(separator: ".").last.map(String.init) ?? "Mac"
        }
    }

    static func fromNowPlaying(_ data: Data) -> Playback? {
        guard let raw = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let title = raw["title"] as? String, !title.isEmpty else { return nil }
        return Playback(source: sourceName(raw["bundleIdentifier"] as? String),
                        title: title,
                        artist: raw["artist"] as? String ?? "",
                        album: raw["album"] as? String ?? "",
                        playing: (raw["playbackRate"] as? Double ?? 0) > 0,
                        position: raw["elapsedTimeNow"] as? Double,
                        duration: raw["duration"] as? Double,
                        artwork: (raw["artworkData"] as? String).flatMap { Data(base64Encoded: $0) })
    }
}

/// Protocol v3 sends supported Cyrillic as UTF-8; older peers receive Latin text.
func displayText(_ input: String, maxBytes: Int, unicode: Bool) -> (Data, Bool) {
    if unicode {
        var result = Data(), changed = false, truncated = false
        for scalar in input.unicodeScalars {
            let value = scalar.value
            let text: String
            if value >= 32 && value <= 126 || value >= 0x400 && value <= 0x47f {
                text = String(scalar)
            } else if CharacterSet.whitespacesAndNewlines.contains(scalar) {
                text = " "
            } else { text = " "; changed = true }
            let bytes = Data(text.utf8)
            if result.count + bytes.count > maxBytes { changed = true; truncated = true; break }
            result.append(bytes)
        }
        if truncated && result.count + 3 <= maxBytes { result.append(contentsOf: Data("...".utf8)) }
        return (result, changed || String(decoding: result, as: UTF8.self) != input)
    }
    let latin = input.applyingTransform(.toLatin, reverse: false) ?? input
    let plain = latin.applyingTransform(.stripDiacritics, reverse: false) ?? latin
    let substitutions = plain.replacingOccurrences(of: "—", with: "-")
        .replacingOccurrences(of: "–", with: "-")
        .replacingOccurrences(of: "’", with: "'")
    var output = String(substitutions.unicodeScalars.map { scalar in
        scalar.value >= 32 && scalar.value <= 126 ? Character(scalar) : " "
    }).split(whereSeparator: \.isWhitespace).joined(separator: " ")
    let truncated = output.utf8.count > maxBytes
    if truncated { output = String(output.prefix(max(0, maxBytes - 3))) + "..." }
    return (Data(output.utf8), output != input || truncated)
}

final class PlaybackStore {
    private(set) var current: Playback?
    private(set) var epoch: UInt32 = 1
    private(set) var revision: UInt32 = 1
    private(set) var sequence: UInt32 = 0
    var changed: ((Bool, Bool) -> Void)?

    func update(_ next: Playback?) {
        let metadataChanged = current?.identity != next?.identity
        let artworkChanged = current?.artwork != next?.artwork
        let stateChanged = current != next
        if metadataChanged { revision = revision == UInt32.max ? 1 : revision + 1 }
        current = next
        if metadataChanged {
            if let next { print("Media: [\(next.source)] \(next.artist) — \(next.title)") }
            else { print("Media: inactive") }
        }
        if stateChanged { changed?(metadataChanged, artworkChanged) }
    }
    func resetConnection() { epoch = epoch == UInt32.max ? 1 : epoch + 1; sequence = 0 }
    func payload(snapshot: Bool, unicode: Bool) -> Data {
        sequence = sequence == UInt32.max ? 1 : sequence + 1
        let item = current
        let fields: [(Data, Bool)] = [
            displayText(item?.title ?? "", maxBytes: 192, unicode: unicode),
            displayText(item?.artist ?? "", maxBytes: 128, unicode: unicode),
            displayText(item?.album ?? "", maxBytes: 192, unicode: unicode),
            displayText(item?.source ?? "Mac", maxBytes: 48, unicode: unicode)
        ]
        let validPosition = item?.position.flatMap { $0.isFinite && $0 >= 0 ? UInt64(min($0 * 1000, 315_360_000_000)) : nil }
        let validDuration = item?.duration.flatMap { $0.isFinite && $0 > 0 ? UInt64(min($0 * 1000, 315_360_000_000)) : nil }
        let position = min(validPosition ?? 0, validDuration ?? UInt64.max)
        let flags: UInt8 = item == nil ? 0 : (validPosition == nil ? 0 : 1) | (validDuration == nil ? 0 : 2) | (fields.contains(where: { $0.1 }) ? 4 : 0)
        var body = Data()
        FNP.put32(item == nil ? 0 : epoch, &body)
        FNP.put32(item == nil ? 0 : revision, &body)
        FNP.put32(sequence, &body)
        body.append(item == nil ? 0 : (item!.playing ? 3 : 2))
        body.append(flags)
        FNP.put16(item?.playing == true ? 1000 : 0, &body)
        FNP.put64(item == nil ? 0 : position, &body)
        FNP.put64(item == nil ? 0 : (validDuration ?? 0), &body)
        FNP.put16(item == nil ? 0 : (SystemVolume.isAvailable ? 31 : 7), &body)
        body.append(0); body.append(0)
        if snapshot {
            for field in fields { FNP.put16(UInt16(field.0.count), &body) }
            for field in fields { body.append(field.0) }
        }
        return body
    }
}

enum Runner {
    static func command(_ executable: String, _ arguments: [String], environment: [String: String]? = nil) -> Data? {
        let task = Process()
        task.executableURL = URL(fileURLWithPath: executable)
        task.arguments = arguments
        if let environment { task.environment = ProcessInfo.processInfo.environment.merging(environment) { _, new in new } }
        let output = Pipe(); task.standardOutput = output
        task.standardError = Pipe()
        do { try task.run(); let data = output.fileHandleForReading.readDataToEndOfFile(); task.waitUntilExit(); return task.terminationStatus == 0 ? data : nil }
        catch { return nil }
    }
    static func desktopSnapshot(_ callback: @escaping (Playback?) -> Void) {
        DispatchQueue.global(qos: .utility).async {
            // nowplaying-cli get-raw drops the MediaRemote timestamp. Its helper
            // calculates elapsedTimeNow from the timestamp and playback rate.
            let bytes = command("/usr/bin/perl", [
                "/opt/homebrew/share/nowplaying-cli/scripts/mediaremote-mini.pl",
                "/opt/homebrew/lib/nowplaying-cli/MediaRemoteMini.dylib",
                "adapter_get_env"
            ], environment: ["MEDIAREMOTEADAPTER_OPTION_now": "1"])
            let state = bytes.flatMap(Playback.fromNowPlaying)
            DispatchQueue.main.async { callback(state) }
        }
    }
    static func desktopCommand(_ action: UInt8) -> Bool {
        if action == 4 || action == 5 {
            return SystemVolume.change(by: action == 4 ? 0.05 : -0.05)
        }
        let names: [UInt8: String] = [1: "togglePlayPause", 2: "previous", 3: "next"]
        guard let name = names[action] else { return false }
        DispatchQueue.global(qos: .userInitiated).async {
            _ = command("/opt/homebrew/bin/nowplaying-cli", [name])
        }
        return true
    }
}
