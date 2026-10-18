Parts
=====

Each .part file is a body template for the Create pane.

    name     = Ocean World        display name (default: file name)
    kind     = terrestrial        star terrestrial giant ice dwarf asteroid comet blackhole
    mass     = 5.97e24            kilograms
    radius   = 6371               kilometres
    orbit    = 1.0                semi-major axis in AU
    ecc      = 0.02               eccentricity
    color1   = #6ec0e8            lit colour   (#rrggbb, #rgb or r,g,b)
    color2   = #123a66            shaded colour
    accent   = #8fd8ff            label and trail colour
    rings    = 1                  optional, draws a ring system
    ring_in  = 1.24               ring inner radius in planet radii
    ring_out = 2.27               ring outer radius in planet radii
    ring_color = 220,210,180,150  optional RGBA
    note     = text               shown in the Library listing

Lines starting with # or ; are comments. Unknown keys are ignored, so a part
written for a newer build still loads.

Click a part in the Library tab to load it into the composer, then press
"Add to sandbox".
