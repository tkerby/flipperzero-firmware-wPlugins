import Foundation
import CoreGraphics
import ImageIO

enum Artwork {
    private static let bayer: [UInt8] = [
        0, 8, 2, 10, 12, 4, 14, 6,
        3, 11, 1, 9, 15, 7, 13, 5
    ]
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
        var grayscale = [UInt8](repeating: 255, count: 45 * 45)
        let space = CGColorSpaceCreateDeviceGray()
        let didDraw = grayscale.withUnsafeMutableBytes { memory -> Bool in
            guard let context = CGContext(data: memory.baseAddress, width: 45, height: 45,
                                          bitsPerComponent: 8, bytesPerRow: 45,
                                          space: space, bitmapInfo: CGImageAlphaInfo.none.rawValue) else { return false }
            context.setFillColor(gray: 1, alpha: 1)
            context.fill(CGRect(x: 0, y: 0, width: 45, height: 45))
            let scale = max(45.0 / Double(image.width), 45.0 / Double(image.height))
            let width = Double(image.width) * scale, height = Double(image.height) * scale
            context.interpolationQuality = .high
            context.draw(image, in: CGRect(x: (45 - width) / 2, y: (45 - height) / 2,
                                           width: width, height: height))
            return true
        }
        guard didDraw else { result.append(contentsOf: [0, 0, 0, 0]); return result }
        result.append(contentsOf: [45, 45, 1, 0])
        var pixels = [UInt8](repeating: 0, count: 270)
        for y in 0..<45 {
            for x in 0..<45 {
                let threshold = Int(bayer[(y % 4) * 4 + x % 4]) * 16 + 8
                if Int(grayscale[y * 45 + x]) < threshold { pixels[y * 6 + x / 8] |= 1 << (x % 8) }
            }
        }
        result.append(contentsOf: pixels)
        return result
    }
}
