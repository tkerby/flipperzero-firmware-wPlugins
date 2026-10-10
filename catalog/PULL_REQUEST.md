# Application Submission

Initial BusterMan Decoder 1.0 submission in Tools. Includes Hex, Base64, ROT13, bounded Brainfuck and Auto chain decoding, text entry/file input, animated artwork and saved XP progression. Optional USB commands support the CyberKing desktop companion. Internal appid megaman is retained to preserve existing progress and companion compatibility.

The 10x10 monochrome icon is adapted from the existing embedded loading-screen artwork. Six original qFlipper screenshots are included unchanged. Source is pinned to an exact public MIT-licensed commit.

# Extra Requirements

Standalone decoding requires only Flipper Zero and its SD card. Optional desktop integration requires USB and CyberKing; it is not required for standalone decoding. Capture transfer reads existing capture files; desktop job execution happens in companion software.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/Tools/megaman/manifest.yml bundle.zip`

# AI usage disclosure (Fill this out)

- Fully AI generated - explain what all the generated code does in moderate detail.

AI tools produced the source under the owner's direction and prepared this submission. decoder.c implements bounded Hex, Base64, ROT13, Brainfuck and layered decoding with explicit output and execution limits. megaman.c implements the Flipper UI, text/file input, results, character animation, bounded XP/progress records and progress synchronization. bridge.c implements read-only capture listing/chunk transfer with CRC and desktop-job messaging through CLI pipes. Embedded artwork is packed in art.h. Host tests exercise decoder vectors/bounds and mocked sync/bridge protocols. Submission work added display metadata, a loading-screen-derived icon, catalog documentation and the manifest without changing application logic.

Validation: official 1.4.3 SDK build and APPCHK passed for target f7/API 87.1; catalog bundling passed. Existing host decoder, sync and bridge tests passed. The owner reports the preceding renamed build works on their physical Flipper; the final metadata/icon rebuild has not separately been tested on hardware here. Full physical USB companion integration remains outside these checks.

# Reviewer Checklist (Don't fill this out, and don't remove it from the template)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
