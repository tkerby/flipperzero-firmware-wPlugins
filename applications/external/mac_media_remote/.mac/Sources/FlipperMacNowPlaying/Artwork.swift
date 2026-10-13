import Foundation
import CoreGraphics
import ImageIO

enum Artwork {
    private static let side = 45

    /// A small Gaussian blur removes details that become stray pixels at 45×45.
    private static func blurred(_ grayscale: [UInt8]) -> [UInt8] {
        var horizontal = [UInt8](repeating: 0, count: grayscale.count)
        var result = horizontal
        for y in 0..<side {
            for x in 0..<side {
                let left = Int(grayscale[y * side + max(0, x - 1)])
                let center = Int(grayscale[y * side + x])
                let right = Int(grayscale[y * side + min(side - 1, x + 1)])
                horizontal[y * side + x] = UInt8((left + 7 * center + right + 4) / 9)
            }
        }
        for y in 0..<side {
            for x in 0..<side {
                let top = Int(horizontal[max(0, y - 1) * side + x])
                let center = Int(horizontal[y * side + x])
                let bottom = Int(horizontal[min(side - 1, y + 1) * side + x])
                result[y * side + x] = UInt8((top + 7 * center + bottom + 4) / 9)
            }
        }
        return result
    }

    static func payload(_ imageData: Data?, epoch: UInt32, revision: UInt32) -> Data {
        var result = Data()
        FNP.put32(epoch, &result); FNP.put32(revision, &result)
        guard let imageData, imageData.count <= 4 * 1024 * 1024,
              let source = CGImageSourceCreateWithData(imageData as CFData, nil),
              let image = CGImageSourceCreateImageAtIndex(source, 0, nil),
              image.width > 0, image.height > 0,
              image.width <= 16384, image.height <= 16384 else {
            result.append(contentsOf: [0, 0, 0, 0]); return result
        }
        var grayscale = [UInt8](repeating: 255, count: side * side)
        let space = CGColorSpaceCreateDeviceGray()
        let didDraw = grayscale.withUnsafeMutableBytes { memory -> Bool in
            guard let context = CGContext(data: memory.baseAddress, width: side, height: side,
                                          bitsPerComponent: 8, bytesPerRow: side,
                                          space: space, bitmapInfo: CGImageAlphaInfo.none.rawValue) else { return false }
            context.setFillColor(gray: 1, alpha: 1)
            context.fill(CGRect(x: 0, y: 0, width: side, height: side))
            let scale = max(Double(side) / Double(image.width), Double(side) / Double(image.height))
            let width = Double(image.width) * scale, height = Double(image.height) * scale
            context.interpolationQuality = .high
            context.draw(image, in: CGRect(x: (Double(side) - width) / 2, y: (Double(side) - height) / 2,
                                           width: width, height: height))
            return true
        }
        guard didDraw else { result.append(contentsOf: [0, 0, 0, 0]); return result }
        let smoothed = blurred(grayscale)
        let sorted = smoothed.sorted()
        let lower = Int(sorted[(sorted.count - 1) * 12 / 100])
        let upper = Int(sorted[(sorted.count - 1) * 88 / 100])
        let cutoff = (48 * lower + 52 * upper) / 100

        result.append(contentsOf: [UInt8(side), UInt8(side), 1, 0])
        var pixels = [UInt8](repeating: 0, count: 270)
        for y in 0..<side {
            for x in 0..<side {
                if Int(smoothed[y * side + x]) <= cutoff { pixels[y * 6 + x / 8] |= 1 << (x % 8) }
            }
        }
        result.append(contentsOf: pixels)
        return result
    }
}
