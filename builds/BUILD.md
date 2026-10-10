# Build record — 2026-10-10

Source commit: 86432309b98aace3ea1d3e2a3fc1cdfbdf2db63f
Display name: BusterMan Decoder
Tool: uFBT 0.2.6
SDK: official release 1.4.3
Target: f7 (7)
API: 87.1
Output size: 21788 bytes
SHA256: 2cac01dba524f7f4f8bf711eb57cc35e4c98529aeb9db9b1ce59b39f542988c6

Compilation, linking and APPCHK passed. Binary metadata contains BusterMan Decoder and no MegaMan Decoder display name. Original app logic, save paths and USB commands were preserved. Host decoder, sync and bridge checks passed; LeakSanitizer was disabled because the execution environment does not support its process inspection. AddressSanitizer and UndefinedBehaviorSanitizer remained enabled for the decoder checks.

No physical device or USB integration test was performed on this rebuilt binary here. Install and test before catalog submission. Internal appid remains megaman, so the build tool emits megaman.fap; the published copy is named BusterMan.fap.
