# RM external-app reliability delta — 22 October 2026

Target: RogueMaster branch `420`, commit `a9237105afde0fc7ab9d7d4ee4025c59c1ebad26`. F7 API remains **88.22**.

This is a separate six-app RM maintenance delta following the Unleashed apps port. These fixes were implemented in this session; they are not a claim of six additional upstream UL commits. The earlier Scheduler/Playlist/Quac/Cross Remote/SubBrute package is unchanged. The two source deltas touch disjoint app directories and can be combined.

## Apply

Use either method, not both:

1. Copy the packaged `applications/` directory into the RM checkout, preserving paths and replacing the listed files.
2. From the RM checkout root, run:

```sh
git apply --check documentation/external_apps_20261022/RM420_External_Apps_Reliability_Delta_20261022.patch
git apply documentation/external_apps_20261022/RM420_External_Apps_Reliability_Delta_20261022.patch
```

For method 2, first copy the packaged documentation directory into the checkout, or use the patch's absolute path instead. Apply to the stated baseline or review any conflicts with later local changes. The manifest contains before/after SHA-256 hashes for every replacement.

**26 source replacements; no source additions or deletions.** No firmware-core, SDK export, RFID, T5577, DND, settings schema, app ID, icon, or category changes. Existing FAPs retain the same firmware API compatibility. These are local RM app-version increments; original application authors and URLs remain in their manifests.

## Apps and behavior

| App | Version | Changes |
| --- | --- | --- |
| Sub-GHz Remote | 1.8 → 1.9 | Checks the hardware async-start result and uses existing error cleanup. Existing hold/release, RAW playback, preset checks, and saved-state handling remain. |
| Sub-GHz WarDriving | 0.1 → 0.2 | Checks async start and missing preset buffers; clears endless-TX on failed transmitter-scene starts. Retains GPS plugin, RX restoration, 32-bit history hashes, and save-back restrictions for history entries. |
| Restaurant Pager | 1.7 → 1.8 | Checks file load/start, validates preset pointers, initializes/clears transmitter pointers, polls hardware completion from the scene, stops on scene exit/free, and displays send failures. Removes the active late RAW callback registration. Returns to the existing menu scene without growing the scene stack. Pager generation and intended repeats are retained. |
| ProtoView | 1.3 → 1.4 | Nonblocking TX serviced by the main loop. Back cancels; long Back exits. Snapshots the bitmap under the scanner mutex so no stack-local feeder or mutable receive bitmap outlives its owner. Restores the prior RX/idle/sleep state, notifies success only on completion, and derives the deadline from waveform duration plus one second. |
| Genie Door Recorder | 3.1 → 3.2 | Sends on a cancellable worker with a five-second stall limit. Uses a locking view model and keeps storage/radio work outside its lock. Advances the selected GNE index only after local TX completion. Checks index writes/sync/close and short version reads; a failed save is retried without RF retransmission. Keeps five repeats, Increment=0, and the GNE file format. Long-OK reset applies only to OK. |
| FlipBoard Signal | 3.12 → 3.13 | Runs the existing Sub-GHz/IR action sequence on a cancellable worker. Back or a new action stops the Sub-GHz send. Checks allocation/deserialization, handles custom preset data, frees the protocol-file handle, and stages/persists dynamic-protocol state. Retains all 15 action mappings and the four IR fallback mappings. |

## Transmission and saved-state details

- RAW playback remains complete unless explicitly cancelled. There is no universal two-second hold or playback limit. FlipBoard's five-second watchdog allows for queued waveform duration, including long pulses; it detects lack of progress rather than imposing a five-second total-send cap. Sub-tick pulse durations retain fractional ticks to avoid accumulated rounding error.
- FlipBoard releases its radio resources before the IR phase. Back can cancel Sub-GHz within the polling interval plus cleanup; an IR record already being sent finishes before the worker joins. Button release alone does not truncate a RAW file. Restaurant Pager's file-generation phase is unchanged.
- FlipBoard retains the original path and loads the existing keystores/rainbow-table paths for dynamic protocols. It writes a complete updated file to `.flip.tmp`, syncs/closes it, and renames it to `.flip.pending` before starting RF. A started TX commits that state even if later cancelled, since a valid frame may already have been emitted. A known failed/cancelled start removes the pending update.
- Recovery moves the original to `.flip.bak`, installs the pending file, then removes the backup. A later load resumes an interrupted rename sequence before another send. Partial `.flip.tmp` files are never used for recovery. Ambiguous states preserve all copies and refuse to send that Sub-GHz file. These steps do not make the FAT filesystem power-loss atomic.
- FlipBoard streams RAW data through the existing encoder. Parsed protocol key files are limited to 16 KiB to avoid unbounded heap allocation; normal key files are much smaller. The original module's internal-radio use is retained; external-radio hardware behavior was not exercised.
- Genie completion means the local radio finished, not that a receiver acknowledged the command. Cancellation can occur after part of a burst has aired. Failed index writes remain pending in the view object and are retried on OK or view entry; closing the app loses an unsaved in-memory retry. The source file is never advanced merely because start was attempted.

## RAM and lifecycle

Genie and FlipBoard each lazily allocate one **4 KiB worker stack**, plus thread bookkeeping. The worker object/stack is retained until the next action or view exit, then joined and freed; there is never an unbounded queue of workers. ProtoView allocates one temporary copy of the transmitted bitmap plus its small feeder context and frees it after DMA stops. The other apps gain no new worker or large buffer. Total on-device peak RAM and stack watermark were not measured.

## Validation

- All six ARM FAP builds and APPCHK import checks passed without compiler/build warnings in the final build.
- Every changed source was hashed before the forced rebuild and matched again during packaging; compiler caching was disabled and target objects/ELFs/FAPs were removed before rebuilding.
- Imports were checked against the exact target commit's API export list, including the embedded WarDriving GPS plugin when present. Updated code strings were verified in the linked binaries.
- Eight host suites passed **3791 assertions** using actual extracted production C with deterministic hardware/storage stand-ins and AddressSanitizer/UndefinedBehaviorSanitizer. Cases cover start/parse/preset failures, cleanup, subsequent sends, completion/cancellation, tick wraparound, immutable waveform snapshots, Genie index/save retry behavior, and FlipBoard staging/recovery failures. Explicit resource-balance checks passed. LeakSanitizer cannot run under this executor.
- The Git patch applies cleanly to the exact baseline, produces byte-identical packaged sources, and passes a reverse-apply check. ZIP CRC and manifest payload hashes were verified.
- No RF, physical SD-card, GPIO/FlipBoard, or RTOS concurrency tests were performed. The host tests model these interfaces and do not establish hardware behavior.

Reproduce the FAP build:

```sh
FBT_NO_SYNC=1 CCACHE_DISABLE=1 ./fbt -j4 fap_subghz_remote_refactored fap_subghz_wardriving fap_meal_pager fap_protoview fap_genie_record fap_flipboard_signal --extra-ext-apps=subghz_wardriving,meal_pager,protoview,genie_record,flipboard_signal SKIP_EXTERNAL=1 DIST_SUFFIX=rm-external6-20261022
```

Reproduce the host tests (requires a C compiler with ASan/UBSan):

```sh
python3 documentation/external_apps_20261022/verify_updates.py /path/to/RM
```

Useful device checks: complete and cancel a short/long RAW send; repeat with an external CC1101 where supported; return to RX after failure; reopen a dynamic key after a successful/cancelled send; interrupt an SD write and confirm recovery/refusal; run repeated send/back/re-enter cycles while observing free heap. In Genie, verify failed starts do not consume an index and save retry does not emit RF.
