# KeyCopier Italia (experimental fork)

An experimental adaptation of [KeyCopier by zinongli](https://github.com/zinongli/KeyCopier) for Flipper Zero, adding three **CISA-style** profiles (3, 4, and 5 positions) with a dedicated pointed-V display and finer 1–15 on-screen depth levels.

**Status:** User-tested prototype; CISA dimensions, depths, angle, and MACS constraints have **not** been independently validated against manufacturer specifications. The 1–15 depth scale is a graphical experimental subdivision, **not** an official CISA key code. Do not rely on it to manufacture functional keys or determine access authorization. Use only with keys and locks you own or have authorization to work with.

## What's different from upstream?

- CISA-style 3, 4, and 5-position experimental formats.
- Dedicated continuous pointed-V drawing routine.
- Nominal V display angle: 110 degrees (visual calibration, not certified tooling geometry).
- Finer 1–15 depth adjustments for these experimental formats.
- Existing selection, Measure, Save, and Load workflow retained **without changes**.

## Build and launch

Install a compatible Flipper Zero development environment and `ufbt`; connect the device, open the directory containing `application.fam`, and run:

```sh
ufbt launch
```

The FAP must be rebuilt for the target firmware/SDK. No precompiled `.fap` is included in this source archive.

## Usage

1. Choose the key manufacturer and profile.
2. Open **Measure** to align the physical key with the on-screen outline and adjust the displayed positions.
3. Save the measurement using the app's **Save** function.
4. Use **Load** to reopen a saved measurement.

## Credits and license

Based on **KeyCopier**, created by [zinongli](https://github.com/zinongli/KeyCopier), with contributions from the upstream community. See the upstream repository for original acknowledgements and history. This fork is independent and is **not** an official CISA or Flipper Devices app.

The original MIT license and copyright notice are preserved in [LICENSE](LICENSE).

## Nota in italiano

Questa e una variante *sperimentale* di KeyCopier con tre profili CISA (3, 4 e 5 posizioni), gola a V grafica di 110 gradi e scala di regolazione 1–15. Quote e livelli non sono certificati dal produttore. Il codice delle funzioni Save/Load e stato lasciato invariato rispetto all'archivio di lavoro testato sul dispositivo.
