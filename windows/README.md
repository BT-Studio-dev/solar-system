# Solar System Sandbox — native Windows builds

Built from the original C sources in this repository's history (commit
`16e0da2` and earlier) with zig cc — no CRT, no redistributables, no runtime
dependencies. Runs on **Windows 7 SP1 and later**.

| File | What it is |
| --- | --- |
| `SolarSystemSandbox-1.0.0-Setup-x64.exe` | installer, **native 64-bit** — use this on Windows 7/8/10/11 x64 |
| `SolarSystemSandbox-1.0.0-Setup.exe` | installer, 32-bit — legacy-safe, runs everywhere via WoW64 |
| `SolarSandbox-x64.exe` | portable 64-bit app — no installer, run from any folder |
| `SolarSandbox.exe` | portable 32-bit app |

The installer carries the application, licence, readme, screenshots and the
sample content folders (Parts, Texture Packs, Mods), installs to
`C:\Program Files\Solar System Sandbox`, adds Start-menu shortcuts and a
Programs-and-Features uninstall entry.

Both architectures were smoke-checked after linking: PE32+/PE32 headers,
GUI subsystem, minimum OS 6.1, and the embedded payload (a 1.7 MB packed
image of the app + content) extracts byte-for-byte with matching CRC-32.

The web app in this repository's root is a separate, browser-based edition.
