import CoreAudio

enum SystemVolume {
    private static func output() -> (AudioDeviceID, AudioObjectPropertyAddress)? {
        var defaultAddress = AudioObjectPropertyAddress(
            mSelector: kAudioHardwarePropertyDefaultOutputDevice,
            mScope: kAudioObjectPropertyScopeGlobal,
            mElement: kAudioObjectPropertyElementMain
        )
        var device = AudioDeviceID(0)
        var size = UInt32(MemoryLayout<AudioDeviceID>.size)
        guard AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject),
                                         &defaultAddress, 0, nil, &size, &device) == noErr else { return nil }
        var volumeAddress = AudioObjectPropertyAddress(
            mSelector: kAudioDevicePropertyVolumeScalar,
            mScope: kAudioDevicePropertyScopeOutput,
            mElement: kAudioObjectPropertyElementMain
        )
        var settable = DarwinBoolean(false)
        guard AudioObjectHasProperty(device, &volumeAddress),
              AudioObjectIsPropertySettable(device, &volumeAddress, &settable) == noErr,
              settable.boolValue else { return nil }
        return (device, volumeAddress)
    }

    static var isAvailable: Bool { output() != nil }

    static func change(by step: Float32) -> Bool {
        guard let (device, volumeAddress) = output() else { return false }
        var address = volumeAddress
        var volume: Float32 = 0
        var size = UInt32(MemoryLayout<Float32>.size)
        guard AudioObjectGetPropertyData(device, &address, 0, nil, &size, &volume) == noErr else { return false }
        volume = min(1, max(0, volume + step))
        return AudioObjectSetPropertyData(device, &address, 0, nil, size, &volume) == noErr
    }
}
