import Foundation

let store = PlaybackStore()
let link = BluetoothLink(store: store)
withExtendedLifetime(link) {
    var busy = false
    Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { _ in
        guard !busy else { return }
        busy = true
        Runner.desktopSnapshot { state in store.update(state); busy = false }
    }.fire()
    RunLoop.main.run()
}
