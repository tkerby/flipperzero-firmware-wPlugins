// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "FlipperMacNowPlaying",
    platforms: [.macOS(.v13)],
    products: [.executable(name: "flipper-mac-now-playing", targets: ["FlipperMacNowPlaying"])],
    targets: [.executableTarget(name: "FlipperMacNowPlaying")]
)
