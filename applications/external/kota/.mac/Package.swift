// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "kota",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "kota",
            path: "Sources/kota",
            // Embedding an Info.plist gives the bare binary its own Bluetooth permission prompt.
            linkerSettings: [.unsafeFlags([
                "-Xlinker", "-sectcreate", "-Xlinker", "__TEXT",
                "-Xlinker", "__info_plist", "-Xlinker", "Info.plist",
            ])]
        ),
    ]
)
