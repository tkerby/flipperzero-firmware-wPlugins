import Foundation
import CoreBluetooth

final class BluetoothLink: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let store: PlaybackStore
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rx: CBCharacteristic?
    private var tx: CBCharacteristic?
    private let parser = FrameStream()
    private var session: UInt32 = 0
    private var nextID: UInt32 = 0
    private var lastPeerID: UInt32 = 0
    private var latestSnapshotID: UInt32 = 0
    private var latestSnapshotSequence: UInt32 = 0
    private var supportsArtwork = false
    private var supportsUnicode = false
    private var commandResults: [UInt32: (Data, Data)] = [:]
    private var outgoing: [Data] = []
    private var active: Data?
    private var offset = 0
    private var writing = false
    private var stage = "waiting for Bluetooth"
    private(set) var ready = false
    private var lastPeer = Date()

    init(store: PlaybackStore) {
        self.store = store
        super.init()
        store.changed = { [weak self] metadata, artwork in self?.publish(metadataChanged: metadata, artworkChanged: artwork) }
        central = CBCentralManager(delegate: self, queue: .main)
        Timer.scheduledTimer(withTimeInterval: 15, repeats: true) { [weak self] _ in self?.heartbeat() }
    }
    private func status(_ value: String) {
        if stage != value { stage = value; print("Bluetooth: \(value)") }
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn { scan() }
        else { status("unavailable (\(central.state.rawValue))"); reset() }
    }
    private func scan() {
        guard central.state == .poweredOn, peripheral == nil else { return }
        status("scanning for Now Playing FAP")
        central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }
    func centralManager(_ central: CBCentralManager, didDiscover candidate: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard peripheral == nil else { return }
        let advertised = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []
        let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? candidate.name ?? ""
        guard advertised.contains(FNP.service) || name == "NP" else { return }
        peripheral = candidate; candidate.delegate = self
        central.stopScan(); status("connecting to \(candidate.name ?? "Flipper")")
        central.connect(candidate)
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        status("discovering service")
        peripheral.discoverServices([FNP.service])
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        status("connection failed: \(error?.localizedDescription ?? "unknown")")
        reset(); self.peripheral = nil; scheduleScan()
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        status("disconnected")
        reset(); self.peripheral = nil; scheduleScan()
    }
    private func scheduleScan() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [weak self] in self?.scan() }
    }
    private func reset() {
        ready = false; supportsArtwork = false; supportsUnicode = false; rx = nil; tx = nil; session = 0; nextID = 0; lastPeerID = 0
        outgoing.removeAll(); active = nil; offset = 0; writing = false
        commandResults.removeAll(); parser.reset(); store.resetConnection()
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard error == nil, let service = peripheral.services?.first(where: { $0.uuid == FNP.service }) else { reconnect("service missing"); return }
        peripheral.discoverCharacteristics([FNP.rx, FNP.tx], for: service)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard error == nil,
              let rx = service.characteristics?.first(where: { $0.uuid == FNP.rx }),
              let tx = service.characteristics?.first(where: { $0.uuid == FNP.tx }),
              rx.properties.contains(.write), tx.properties.contains(.indicate) else { reconnect("incompatible characteristics"); return }
        self.rx = rx; self.tx = tx
        status("subscribing; confirm Bluetooth pairing if prompted")
        peripheral.setNotifyValue(true, for: tx)
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == FNP.tx, characteristic.isNotifying else { reconnect("subscription failed"); return }
        session = UInt32.random(in: 1...UInt32.max)
        var hello = Data([1, 3]); FNP.put16(768, &hello); FNP.put16(128, &hello); FNP.put16(0, &hello); FNP.put32(1, &hello)
        status("handshaking")
        send(1, hello)
    }
    private func send(_ type: UInt8, _ payload: Data) {
        guard session != 0, nextID < UInt32.max else { reconnect("counter exhausted"); return }
        nextID += 1
        if type == 16 { latestSnapshotID = nextID; latestSnapshotSequence = FNP.u32(payload, 8) }
        outgoing.append(Frame(type: type, session: session, id: nextID, payload: payload).encoded())
        pump()
    }
    private func pump() {
        guard !writing, let device = peripheral, let characteristic = rx else { return }
        if active == nil {
            guard !outgoing.isEmpty else { return }
            active = outgoing.removeFirst(); offset = 0
        }
        guard let frame = active else { return }
        let end = min(offset + 20, frame.count)
        writing = true
        device.writeValue(frame.subdata(in: offset..<end), for: characteristic, type: .withResponse)
    }
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == FNP.rx, let frame = active else {
            reconnect("write failed: \(error?.localizedDescription ?? "invalid state")")
            return
        }
        writing = false; offset += min(20, frame.count - offset)
        if offset >= frame.count { active = nil; offset = 0 }
        pump()
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == FNP.tx, let bytes = characteristic.value else { reconnect("receive failed"); return }
        for frame in parser.feed(bytes) { receive(frame) }
    }
    private func receive(_ frame: Frame) {
        guard frame.session == session, frame.id != 0 else { reconnect("session mismatch"); return }
        if frame.type == 32, let cached = commandResults[frame.id] {
            if cached.0 == frame.payload { send(33, cached.1) } else { reconnect("command ID reused") }
            return
        }
        guard frame.id > lastPeerID else { reconnect("out of order frame"); return }
        lastPeerID = frame.id; lastPeer = Date()
        switch frame.type {
        case 2:
            guard stage == "handshaking", frame.payload.count == 12, (1...3).contains(FNP.byte(frame.payload, 0)),
                  FNP.byte(frame.payload, 1) == 0 else { reconnect("HELLO rejected"); return }
            supportsUnicode = FNP.byte(frame.payload, 0) == 3
            supportsArtwork = FNP.byte(frame.payload, 0) >= 2
            status("syncing")
            snapshot()
        case 18:
            guard frame.payload.count == 8 else { reconnect("bad snapshot ACK"); return }
            if FNP.u32(frame.payload, 0) == latestSnapshotID && FNP.u32(frame.payload, 4) == latestSnapshotSequence {
                ready = true; status("ready")
                sendArtwork()
            }
        case 32: command(frame)
        case 48: snapshot()
        case 65: break
        case 126, 127: reconnect("peer closed or rejected protocol")
        default: reconnect("unexpected frame")
        }
    }
    private func snapshot() {
        guard stage == "syncing" || stage == "ready" else { return }
        ready = false
        send(16, store.payload(snapshot: true, unicode: supportsUnicode))
    }
    private func publish(metadataChanged: Bool, artworkChanged: Bool) {
        guard stage == "ready" else { return }
        if metadataChanged { snapshot() }
        else { send(17, store.payload(snapshot: false, unicode: supportsUnicode)); if artworkChanged { sendArtwork() } }
    }
    private func sendArtwork() {
        guard supportsArtwork, let item = store.current else { return }
        send(19, Artwork.payload(item.artwork, epoch: store.epoch, revision: store.revision))
    }
    private func command(_ frame: Frame) {
        guard frame.payload.count == 12 else { reconnect("bad command"); return }
        let target = FNP.u32(frame.payload, 0), action = FNP.byte(frame.payload, 8)
        let status: UInt8
        if !ready { status = 7 }
        else if store.current == nil { status = 1 }
        else if target != store.epoch { status = 2 }
        else if !(1...5).contains(action) { status = 3 }
        else { status = Runner.desktopCommand(action) ? 0 : 3 }
        var response = Data(); FNP.put32(frame.id, &response)
        response.append(status); response.append(0); FNP.put16(0, &response); FNP.put32(store.current == nil ? 0 : store.epoch, &response)
        commandResults[frame.id] = (frame.payload, response)
        if commandResults.count > 32, let oldest = commandResults.keys.min() { commandResults.removeValue(forKey: oldest) }
        send(33, response)
    }
    private func heartbeat() {
        guard stage == "ready" else { return }
        if Date().timeIntervalSince(lastPeer) > 35 { reconnect("peer timeout"); return }
        var cookie = Data(); FNP.put32(UInt32.random(in: 0...UInt32.max), &cookie)
        send(64, cookie)
        send(17, store.payload(snapshot: false, unicode: supportsUnicode))
    }
    private func reconnect(_ reason: String) {
        status(reason)
        if let device = peripheral { central.cancelPeripheralConnection(device) }
        else { reset(); scheduleScan() }
    }
}
