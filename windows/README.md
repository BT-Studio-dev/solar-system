# Solar System Sandbox — native Windows builds

Built from the original C sources in this repository's history (commit
`16e0da2` and earlier) with zig cc — no CRT, no redistributables, no runtime
dependencies. Runs on **Windows 7 SP1 and later**.

| File | Size | What it is |
| --- | --- | --- |
| **`SolarSystemSandbox-1.0.0-Setup.exe`** | 2.7 MB | **one installer for every Windows 7+ PC** — carries both the 32-bit and the native 64-bit app and installs whichever matches your Windows (checked at runtime via `IsWow64Process`); the app always installs as `SolarSandbox.exe` |
| `SolarSandbox.exe` | 646 KB | portable 32-bit app — run from any folder, no installer |
| `SolarSandbox-x64.exe` | 621 KB | portable native 64-bit app |

The installer carries the licence, readme, screenshots and the sample content
folders (Parts, Texture Packs, Mods), installs to
`C:\Program Files\Solar System Sandbox` (or `Program Files (x86)` on 32-bit
Windows), adds Start-menu shortcuts and a Programs-and-Features uninstall
entry, and lets you pick a colour theme before installing.

## Rendering backends (dual, automatic)

Each app ships **two renderers** and picks between them on its own:

- **GDI+** — the default. On machines where GDI+ misbehaves (a known cause of
  permanently black windows on some Windows 7 configurations), the app
  detects the failure on the very first frame and **switches itself to the
  software renderer** automatically.
- **Software rasteriser** — a built-in fallback that draws through plain GDI
  and works on any display configuration.

You can also force it yourself: start the app with the `--soft` flag, or use
**View → Software rendering (troubleshooting)** inside the app. Whichever
renderer is active is named in the startup log (Help → Open startup log).

Verified after linking: PE32 header, GUI subsystem, minimum OS 6.1, and the
embedded payload extracts byte-for-byte with matching CRC-32 (both app
architectures present, both with the dual renderer). The setup executable
itself is 32-bit, so it starts on every Windows 7 installation — 32- and
64-bit alike.

The web app in this repository's root is a separate, browser-based edition.
