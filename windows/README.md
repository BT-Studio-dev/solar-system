# Solar System Sandbox — native Windows builds

Built from the original C sources in this repository's history (commit
`16e0da2` and earlier) with zig cc — no CRT, no redistributables, no runtime
dependencies. Runs on **Windows 7 SP1 and later**.

| File | Size | What it is |
| --- | --- | --- |
| **`SolarSystemSandbox-1.0.0-Setup.exe`** | 2.1 MB | **one installer for every Windows 7+ PC** — carries both the 32-bit and the native 64-bit app and installs whichever matches your Windows (checked at runtime via `IsWow64Process`); the app always installs as `SolarSandbox.exe` |
| `SolarSandbox.exe` | 320 KB | portable 32-bit app — run from any folder, no installer |
| `SolarSandbox-x64.exe` | 294 KB | portable native 64-bit app |

The installer carries the licence, readme, screenshots and the sample content
folders (Parts, Texture Packs, Mods), installs to
`C:\Program Files\Solar System Sandbox` (or `Program Files (x86)` on 32-bit
Windows), adds Start-menu shortcuts and a Programs-and-Features uninstall
entry, and lets you pick a colour theme before installing.

Verified after linking: PE32 header, GUI subsystem, minimum OS 6.1, and the
2.0 MB embedded payload extracts byte-for-byte with matching CRC-32 (both
app architectures present). The setup executable itself is 32-bit, so it
starts on every Windows 7 installation — 32- and 64-bit alike.

The web app in this repository's root is a separate, browser-based edition.
