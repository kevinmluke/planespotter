# PlaneSpotter enclosure

A wedge-shaped desk case that leans the 5.79" e-paper display back by 20°.
Overall size: about 176 × 88 × 62 mm.

| | |
|---|---|
| ![front](assembly_front.svg) | ![back](assembly_back.svg) |
| ![exploded](exploded.svg) | |

## Files

| Folder | What it's for |
|---|---|
| `stl/` | Send these to the printer. They're already in printing orientation |
| `step/` | Editable models for Fusion 360, FreeCAD or Onshape (in assembled position) |
| `enclosure.py` | The source. Change a measurement at the top, then run `python3 enclosure/enclosure.py` (needs `pip install build123d`) to regenerate everything |

## Before you print

This model hasn't been test-printed yet. Check two things against your parts:

1. **ESP32 board size.** The base cradle assumes 70 × 29 mm. Measure your board
   and change `ESP_L` / `ESP_W` if needed.
2. **Screen window and ribbon.** The window and the ribbon cutout use the
   dimensions in Good Display's mechanical drawing for the GDEY0579T93: borders of
   2.80 mm on the left, 9.12 mm on the ribbon side and 4.60 mm top and bottom, and
   a 34.10 mm wide ribbon. Print the bezel first, lay the panel in its pocket, and
   check that no pixels are hidden and the ribbon lies flat. Adjust
   `ACTIVE_LEFT_BORDER`, `ACTIVE_BOTTOM_BORDER` or `FPC_WIDTH` if needed.

## Printing

PLA or PETG, 0.2 mm layers, 3 walls, 15–20% infill.

| Part | Orientation | Supports |
|---|---|---|
| `bezel.stl` | Face down (as exported) | None |
| `backer.stl` | Flat | None |
| `body.stl` | Top down, 61.5 mm tall (as exported) | None |
| `base.stl` | Flat | None |
| `button_cap.stl` | Top down | None |

The body prints upside down: the flat strip of the top lies on the plate and the
open bottom faces up. Small 45° ramps under the screw bosses and the bezel screw
tabs hold them up while printing, so no supports are needed. The look in the
photos comes from black "sparkle" or "galaxy" PLA, or a fuzzy-skin slicer setting
on the outside walls.

## Hardware

- 2 × M2.5 × 10 mm screws (bezel and backer)
- 4 × M2.5 × 8 mm screws (base)
- 4 rubber feet
- 6 × 6 mm tactile button (wired to GPIO 0 and GND)
- Double-sided tape (DESPI-C579 adapter, ESP32 board)
- Optional: a thin foam pad (0.3 mm) behind the panel

The screw holes are sized for self-tapping into plastic. Metal self-tapping
screws grip better than nylon ones.

## Assembly

1. **Button.** Drop the button cap into the hole in the top from inside. Push the
   tactile button up into the tube under it until it touches the cap, then fix it
   with a dab of hot glue. Solder or connect its two wires.
2. **ESP32.** Place the board **upside down** (pins up) in the base cradle, USB-C
   end at the back, and fix it with a strip of double-sided tape.
3. **Panel.** Lay the bezel face down. Put the glass, screen side down, into the
   pocket, with the ribbon cable on the right (seen from the front) lying in its
   cutout. Lay the backer flat on the panel about 6 mm lower than its final
   position, clear of the two hooks at the top, then slide it up until it stops
   under the hooks.
4. **Adapter.** Fold the ribbon cable gently around the backer's right edge,
   connect it to the DESPI-C579, and stick the adapter to the back of the backer
   near that edge. Connect the jumper wires (see the wiring table in the main
   README).
5. **Bezel into body.** Hook the top of the bezel under the body's top edge, swing
   the bottom in, and fit the 2 × 10 mm screws from inside, through the tabs at
   the bottom. The tabs also clamp the backer's left and right edges.
6. **Base.** Put the base in from below, with the USB-C ports lined up with the
   opening at the back, and fit the 4 × 8 mm screws. Stick on the rubber feet.
