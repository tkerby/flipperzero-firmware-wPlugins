import Foundation

enum FNP {
    static let service = CBUUID(string: "8f9a1000-6d8a-4f1b-a6e6-3b4c5d6e7f80")
    static let rx = CBUUID(string: "8f9a1001-6d8a-4f1b-a6e6-3b4c5d6e7f80")
    static let tx = CBUUID(string: "8f9a1002-6d8a-4f1b-a6e6-3b4c5d6e7f80")
    static let maxPayload = 768

    static func byte(_ data: Data, _ index: Int) -> UInt8 {
        data[data.index(data.startIndex, offsetBy: index)]
    }

    static func crc(_ bytes: Data) -> UInt32 {
        var result: UInt32 = 0xffff_ffff
        for byte in bytes {
            result ^= UInt32(byte)
            for _ in 0..<8 { result = (result >> 1) ^ ((result & 1) == 1 ? 0xedb8_8320 : 0) }
        }
        return result ^ 0xffff_ffff
    }
    static func u16(_ data: Data, _ index: Int) -> UInt16 {
        UInt16(byte(data, index)) | (UInt16(byte(data, index + 1)) << 8)
    }
    static func u32(_ data: Data, _ index: Int) -> UInt32 {
        UInt32(byte(data, index)) | (UInt32(byte(data, index + 1)) << 8) |
            (UInt32(byte(data, index + 2)) << 16) | (UInt32(byte(data, index + 3)) << 24)
    }
    static func put16(_ value: UInt16, _ data: inout Data) {
        data.append(UInt8(truncatingIfNeeded: value)); data.append(UInt8(truncatingIfNeeded: value >> 8))
    }
    static func put32(_ value: UInt32, _ data: inout Data) {
        for offset in stride(from: 0, to: 32, by: 8) { data.append(UInt8(truncatingIfNeeded: value >> offset)) }
    }
    static func put64(_ value: UInt64, _ data: inout Data) {
        for offset in stride(from: 0, to: 64, by: 8) { data.append(UInt8(truncatingIfNeeded: value >> offset)) }
    }
}

import CoreBluetooth

struct Frame {
    let type: UInt8
    let session: UInt32
    let id: UInt32
    let payload: Data

    func encoded() -> Data {
        precondition(!payload.isEmpty || type != 16)
        precondition(payload.count <= FNP.maxPayload && session != 0 && id != 0)
        var result = Data("FNP1".utf8)
        result.append(1); result.append(type)
        FNP.put16(UInt16(payload.count), &result)
        FNP.put32(session, &result); FNP.put32(id, &result)
        result.append(payload)
        FNP.put32(FNP.crc(result.subdata(in: 4..<result.count)), &result)
        return result
    }

    static func decode(_ bytes: Data) -> Frame? {
        guard bytes.count >= 20, bytes.prefix(4) == Data("FNP1".utf8), FNP.byte(bytes, 4) == 1 else { return nil }
        let length = Int(FNP.u16(bytes, 6))
        guard length <= FNP.maxPayload, bytes.count == length + 20,
              FNP.crc(Data(bytes.dropFirst(4).prefix(12 + length))) == FNP.u32(bytes, 16 + length) else { return nil }
        return Frame(type: FNP.byte(bytes, 5), session: FNP.u32(bytes, 8), id: FNP.u32(bytes, 12), payload: Data(bytes.dropFirst(16).prefix(length)))
    }
}

final class FrameStream {
    private var buffer = Data()
    func reset() { buffer.removeAll(keepingCapacity: true) }
    func feed(_ chunk: Data) -> [Frame] {
        guard chunk.count <= 128 else { reset(); return [] }
        buffer.append(chunk)
        var frames: [Frame] = []
        while buffer.count >= 4 {
            if buffer.prefix(4) != Data("FNP1".utf8) { buffer.removeFirst(); continue }
            if buffer.count < 16 { break }
            let length = Int(FNP.u16(buffer, 6))
            if FNP.byte(buffer, 4) != 1 || length > FNP.maxPayload { buffer.removeFirst(); continue }
            if buffer.count < length + 20 { break }
            let candidate = buffer.prefix(length + 20)
            if let frame = Frame.decode(Data(candidate)) { frames.append(frame); buffer.removeFirst(length + 20) }
            else { buffer.removeFirst() }
        }
        if buffer.count > 788 { reset() }
        return frames
    }
}
