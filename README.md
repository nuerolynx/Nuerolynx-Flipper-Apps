# Nuerolynx Flipper Apps

This repository is the external application source used by [Nuerolynx Flipper Firmware](https://github.com/nuerolynx/Nuerolynx-Flipper-Firmware). It follows the Momentum Apps bundle and carries the small set of changes required by the Nuerolynx build.

Most people do not need to clone this repository. The complete firmware update package already contains the compiled applications. Use the firmware repository if you want to install Nuerolynx on a Flipper Zero.

## Nuerolynx changes

The main addition is the NLX Electrical Calculator. It combines Ohm's law with two conductor voltage drop and wire sizing calculations for field and bench work. The app reports delivered voltage, wire loss, maximum run length, and a sizing verdict while keeping the limits of a handheld design aid clear.

The bundle also contains small identity and default value changes that keep included applications consistent with the Nuerolynx firmware.

## Build use

The firmware repository records this repository as a Git submodule. Clone the firmware recursively to retrieve the exact app revision used by a build.

```powershell
git clone --recursive https://github.com/nuerolynx/Nuerolynx-Flipper-Firmware.git
```

This source snapshot keeps the code, application manifests, runtime resources, licenses, and text documentation used for development. Large reference PDFs, demo videos, screenshots, and layered design files that are not used by the firmware build are omitted. See [SOURCE_ASSETS.md](SOURCE_ASSETS.md) for the exact policy and the upstream location of those materials.

## Credits and licenses

This repository is derived from [Momentum Apps](https://github.com/Next-Flip/Momentum-Apps) and contains applications from many independent authors. Original copyright notices, per application licenses, and author credits remain with their respective files.

The NLX Electrical Calculator extends the original MIT licensed VoltCalc by Andrew Diamond. Its original copyright and license are retained in `voltage_calculator/LICENSE`.
