# Fonts used by the screenshot harness

These files are **not shipped in the Windows application** (Windows uses the
native Segoe UI / Consolas). They are used only by `tools/bake_fonts.py` to
generate `src/font_baked.h`, the glyph atlas for the headless software
renderer used by the automated screenshot tests.

- Open Sans (Light / Regular / SemiBold) - Apache License 2.0
- Roboto Mono (Regular) - SIL Open Font License 1.1

Downloaded via npm packages `@fontsource/open-sans` and
`@fontsource/roboto-mono`. See LICENSE-OFL.txt.
