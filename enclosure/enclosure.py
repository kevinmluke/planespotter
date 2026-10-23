#!/usr/bin/env python3
"""PlaneSpotter desk enclosure: a wedge that leans the 5.79" e-paper back by 20 deg.

    python3 enclosure/enclosure.py          (needs: pip install build123d)

Writes printable STLs to enclosure/stl/, STEP files (for Fusion 360, FreeCAD,
Onshape...) to enclosure/step/, and drawings (assembly_front/back.svg, exploded.svg).

Five parts, printed separately:
  bezel       front frame with the window; the panel drops into a pocket on its back
  backer      plate behind the panel; holds it in the pocket, slot for the ribbon cable
  body        the wedge: side walls, top, back, USB-C opening, button guide
  base        bottom plate with a cradle for the ESP32-S3 board
  button_cap  plunger that presses the 6 mm tactile button from the top

Hardware: 6x M2.5 screws (2x 10 mm for bezel/backer, 4x 8 mm for the base),
4 rubber feet, a 6x6 mm tactile button, double-sided tape.

Coordinates: X = width (left to right as seen from the front), Y = depth
(front to back), Z = up. The "face plane" is where the back of the bezel meets
the front edges of the body; it passes through the front-bottom edge and leans
back by TILT degrees.
"""
import math
import os

from build123d import (Axis, Box, Cone, Cylinder, ExportSVG, Location, Plane, Polygon, Pos, Rectangle, Rot,
                       export_step, export_stl, extrude, fillet)

HERE = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------------------
# Parameters (mm)

# Good Display GDEY0579T93
PANEL_W, PANEL_H, PANEL_T = 150.92, 56.94, 1.0
ACTIVE_W, ACTIVE_H = 139.00, 47.74
# Where the active area sits on the glass: the ribbon cable leaves from the RIGHT
# short edge (seen from the front) and that end has the wider border. Estimated
# from the product photos; check against the datasheet or your panel and adjust.
ACTIVE_LEFT_BORDER = 2.6          # glass left edge -> active area (right border = the rest)
ACTIVE_BOTTOM_BORDER = 4.6        # glass bottom edge -> active area
FPC_WIDTH = 24.0                  # ribbon cable width where it leaves the glass (with margin)

WINDOW_MARGIN = 1.0               # window is this much larger than the active area, per side
FIT = 0.25                        # clearance around the glass in its pocket, per side

TILT = 20.0                       # face leans back by this much from vertical
WALL = 2.4                        # body wall thickness
SKIN = 2.0                        # bezel thickness in front of the glass
POCKET = PANEL_T + 0.3            # pocket depth (glass + a thin foam pad)
BACKER_T = 2.0
EAR_T = 5.0                       # screw ears inside the body that clamp bezel + backer

BODY_W = 176.0                    # overall width
FACE_H = 65.5                     # bezel height along the sloped face
DEPTH = 88.0                      # footprint depth
TOP_DEPTH = 17.6                  # depth of the flat top
REAR_H = 22.0                     # height of the vertical part of the back (USB-C opening)

# ESP32-S3-DevKitC-1 (N16R8 clone, dual USB-C). It sits UPSIDE DOWN in the base
# cradle (pins up, so the jumper wires plug in from above), USB-C end at the back.
ESP_L, ESP_W = 70.0, 29.0         # measure your board; clones vary by a mm or two
ESP_X = 30.0                      # board centre, from the middle of the case
USB_SLOT_W, USB_SLOT_H = 26.0, 9.5

BUTTON_X = -60.0                  # button on the top, left side

s, c = math.sin(math.radians(TILT)), math.cos(math.radians(TILT))
TOP_Z = FACE_H * c
TOP_Y0 = FACE_H * s
FACE = Plane(origin=(0, 0, 0), x_dir=(1, 0, 0), z_dir=(0, -c, s))  # local y: up the face, local z: out
F = FACE.location

# Panel and window positions in face coordinates (window centred on the bezel)
VC = FACE_H / 2                                       # vertical centre of the face
active_cx = -PANEL_W / 2 + ACTIVE_LEFT_BORDER + ACTIVE_W / 2
active_cy = -PANEL_H / 2 + ACTIVE_BOTTOM_BORDER + ACTIVE_H / 2
PANEL_X = -active_cx                                  # glass centre so that the active area is centred
PANEL_Y = VC - active_cy
POCKET_W, POCKET_H = PANEL_W + 2 * FIT, PANEL_H + 2 * FIT
BOSS_X = BODY_W / 2 - WALL - 3.25
BOSS_DY = 22.5
# Two screws at the bottom; at the top the bezel hooks under the body's top wall instead
# (screws up there could not be reached with a screwdriver).
BOSSES = [(sx * BOSS_X, VC - BOSS_DY) for sx in (-1, 1)]
BACKER_Y0, BACKER_Y1 = 3.5, FACE_H - 3.0
TONGUE_X = 33.0                   # two T-shaped tongues, at +/- this x
TONGUE_Y0 = 57.5                  # where the tongues start (face coordinates)
NECK_HW, HEAD_HW = 13.0, 20.0     # half widths of the tongue neck and head
HEAD_GAP = 0.15                   # clearance between backer and tongue heads


def box(x0, x1, y0, y1, z0, z1):
    return Pos((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2) * Box(x1 - x0, y1 - y0, z1 - z0)


def cyl(x, y, z0, z1, r):
    return Pos(x, y, (z0 + z1) / 2) * Cylinder(r, z1 - z0)


# ---------------------------------------------------------------------------
# Bezel (built in face coordinates: z = 0 is the face plane, +z towards the viewer)

def make_bezel():
    plate = box(-BODY_W / 2, BODY_W / 2, 0, FACE_H, 0, POCKET + SKIN)
    front = plate.faces().sort_by(Axis.Z)[-1]
    plate = fillet(front.outer_wire().edges(), 1.5)
    # Pocket for the glass, with room for the ribbon cable on the right
    plate -= Pos(PANEL_X, PANEL_Y, POCKET / 2 - 0.01) * Box(POCKET_W, POCKET_H, POCKET + 0.02)
    plate -= box(PANEL_X + PANEL_W / 2, PANEL_X + PANEL_W / 2 + 4.5, PANEL_Y - FPC_WIDTH / 2,
                 PANEL_Y + FPC_WIDTH / 2, -0.01, POCKET)
    # Window with a bevel that opens towards the viewer
    ww, wh = ACTIVE_W + 2 * WINDOW_MARGIN, ACTIVE_H + 2 * WINDOW_MARGIN
    window = extrude(Plane.XY.offset(POCKET - 0.01) * Pos(0, VC) * Rectangle(ww, wh), amount=SKIN + 0.02, taper=-40)
    plate -= window
    # Screw bosses behind the face (they pass through the backer and meet the body's ears)
    for x, y in BOSSES:
        plate += cyl(x, y, -BACKER_T, 0.01, 2.7)
        plate -= cyl(x, y, -BACKER_T - 0.01, POCKET + SKIN - 0.8, 1.1)  # M2.5 self-tapping pilot
    # Hooks: T-shaped tongues that slide under the body's top wall. Their top is cut
    # horizontal, flush under the wall, so the bezel can't tip forward.
    under_wall = Location((0, 0, 0)) * box(-BODY_W, BODY_W, -50, 200, TOP_Z - WALL - 0.05, 300)
    for sx in (-1, 1):
        x = sx * TONGUE_X
        tongue = box(x - NECK_HW, x + NECK_HW, TONGUE_Y0, FACE_H, -BACKER_T - HEAD_GAP, 0.01)
        tongue += box(x - HEAD_HW, x + HEAD_HW, TONGUE_Y0, FACE_H, -BACKER_T - HEAD_GAP - 3.0, -BACKER_T - HEAD_GAP)
        tongue = F.inverse() * ((F * tongue) - under_wall)
        plate += tongue
    return plate


def make_backer():
    x_in = BODY_W / 2 - WALL - 0.4
    plate = box(-x_in, x_in, BACKER_Y0, BACKER_Y1, -BACKER_T, 0)
    for x, y in BOSSES:
        plate -= cyl(x, y, -BACKER_T - 1, 1, 3.0)
    # Notches for the bezel's tongue necks (the backer slides up under the tongue heads)
    for sx in (-1, 1):
        x = sx * TONGUE_X
        plate -= box(x - NECK_HW - 0.3, x + NECK_HW + 0.3, TONGUE_Y0 - 0.3, FACE_H, -BACKER_T - 1, 1)
    # Slot for the ribbon cable to fold behind the panel
    edge = PANEL_X + PANEL_W / 2
    plate -= box(edge - 3, edge + 5, PANEL_Y - FPC_WIDTH / 2 - 1, PANEL_Y + FPC_WIDTH / 2 + 1, -BACKER_T - 1, 1)
    return plate


# ---------------------------------------------------------------------------
# Body (world coordinates)

def _intersect(p1, d1, p2, d2):
    """Intersection of lines p1 + t*d1 and p2 + u*d2 in 2D."""
    det = d1[0] * (-d2[1]) - d1[1] * (-d2[0])
    t = ((p2[0] - p1[0]) * (-d2[1]) - (p2[1] - p1[1]) * (-d2[0])) / det
    return (p1[0] + t * d1[0], p1[1] + t * d1[1])


def make_body():
    top_end = (TOP_Y0 + TOP_DEPTH, TOP_Z)
    outer = [(0, 0), (TOP_Y0, TOP_Z), top_end, (DEPTH, REAR_H), (DEPTH, 0)]
    # Inner profile: each outer line moved inwards by WALL; open at the front and the bottom
    slope_d = (DEPTH - top_end[0], REAR_H - top_end[1])
    ln = math.hypot(*slope_d)
    slope_n = (slope_d[1] / ln, -slope_d[0] / ln)  # points into the body
    slope_p = (top_end[0] + slope_n[0] * WALL, top_end[1] + slope_n[1] * WALL)
    face_p, face_d = (-c * 1.0, s * 1.0), (s, c)   # face line pushed 1 mm outwards: opens the front
    top_p, top_d = (0, TOP_Z - WALL), (1, 0)
    rear_p, rear_d = (DEPTH - WALL, 0), (0, 1)
    bot_p, bot_d = (0, -1.0), (1, 0)
    inner = [_intersect(face_p, face_d, bot_p, bot_d), _intersect(face_p, face_d, top_p, top_d),
             _intersect(top_p, top_d, slope_p, slope_d), _intersect(slope_p, slope_d, rear_p, rear_d),
             _intersect(rear_p, rear_d, bot_p, bot_d)]
    body = extrude(Plane.YZ * Polygon(*outer, align=None), amount=BODY_W / 2, both=True)
    body -= extrude(Plane.YZ * Polygon(*inner, align=None), amount=BODY_W / 2 - WALL, both=True)

    # Ears that clamp bezel + backer (screws go in from inside, towards the front)
    for x, y in BOSSES:
        side = 1 if x > 0 else -1
        x_wall = side * (BODY_W / 2 - WALL)
        ear = F * box(min(x_wall, x_wall - side * 6.5), max(x_wall, x_wall - side * 6.5), y - 4.5, y + 4.5,
                      -BACKER_T - EAR_T, -BACKER_T)
        ear -= F * cyl(x, y, -BACKER_T - EAR_T - 1, 0, 1.45)  # M2.5 clearance
        body += ear

    # Bosses for the base plate screws
    for sx in (-1, 1):
        for y in (45.0, DEPTH - WALL - 3.5):
            bx = sx * (BODY_W / 2 - WALL - 3.5)
            body += cyl(bx, y, WALL, 12.0, 3.5)
            body += box(min(bx, sx * (BODY_W / 2 - WALL)), max(bx, sx * (BODY_W / 2 - WALL)), y - 3.5, y + 3.5, WALL, 12.0)
            body -= cyl(bx, y, WALL - 0.01, 10.5, 1.1)

    # USB-C opening in the back (open at the bottom so the plug's overmould fits)
    body -= box(ESP_X - USB_SLOT_W / 2, ESP_X + USB_SLOT_W / 2, DEPTH - WALL - 1, DEPTH + 1, -1, USB_SLOT_H)

    # Button: hole in the top and a guide tube underneath (fits a 6x6 mm tactile switch)
    by = TOP_Y0 + TOP_DEPTH / 2
    tube_top = TOP_Z - WALL
    body += cyl(BUTTON_X, by, tube_top - 7.0, tube_top + 0.01, 5.8) - cyl(BUTTON_X, by, tube_top - 7.1, tube_top + 0.1, 4.3)
    body -= cyl(BUTTON_X, by, tube_top - 1, TOP_Z + 1, 3.05)
    return body


def make_base():
    x_in = BODY_W / 2 - WALL - 0.3
    plate = box(-x_in, x_in, 1.0, DEPTH - WALL - 0.3, 0, WALL)
    for sx in (-1, 1):
        for y in (45.0, DEPTH - WALL - 3.5):
            bx = sx * (BODY_W / 2 - WALL - 3.5)
            plate -= cyl(bx, y, -1, WALL + 1, 1.45)
            plate -= Pos(bx, y, 0) * Cone(2.8, 1.45, 1.35, align=None)  # countersink from below
    # Cradle for the ESP32-S3 board (upside down, USB-C end against the back wall)
    y1 = DEPTH - WALL
    y0 = y1 - ESP_L - 0.6
    x0, x1 = ESP_X - ESP_W / 2 - 0.3, ESP_X + ESP_W / 2 + 0.3
    for wall in (box(x0 - 1.6, x0, y0 - 1.6, y1 - 6, WALL, WALL + 3), box(x1, x1 + 1.6, y0 - 1.6, y1 - 6, WALL, WALL + 3),
                 box(x0 - 1.6, x1 + 1.6, y0 - 1.6, y0, WALL, WALL + 3)):
        plate += wall
    return plate


def make_button_cap():
    cap = cyl(0, 0, 0, 1.2, 1.5)           # stem that presses the switch
    cap += cyl(0, 0, 1.2, 2.2, 4.0)        # flange (stops it falling out)
    cap += cyl(0, 0, 2.2, 2.2 + WALL + 1.6, 2.8)
    top = cap.faces().sort_by(Axis.Z)[-1]
    return fillet(top.edges(), 0.8)


# ---------------------------------------------------------------------------

def main():
    os.makedirs(os.path.join(HERE, "stl"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "step"), exist_ok=True)
    bezel, backer, body, base, cap = make_bezel(), make_backer(), make_body(), make_base(), make_button_cap()

    # Assembled positions (world coordinates)
    asm = {"bezel": F * bezel, "backer": F * backer, "body": body, "base": base,
           "button_cap": Pos(BUTTON_X, TOP_Y0 + TOP_DEPTH / 2, TOP_Z - WALL - 2.2) * cap}
    panel = F * Pos(PANEL_X, PANEL_Y, POCKET - PANEL_T / 2) * Box(PANEL_W, PANEL_H, PANEL_T)
    esp = Pos(ESP_X, DEPTH - WALL - ESP_L / 2, WALL + 3.2 + 0.8) * Box(ESP_W, ESP_L, 1.6)  # PCB on its module

    # Interference check between parts that touch (should all be ~0)
    pairs = [("bezel", "body"), ("backer", "body"), ("base", "body"), ("backer", "bezel"), ("button_cap", "body")]
    for a, b in pairs:
        v = (asm[a] & asm[b]).volume
        print(f"overlap {a:10} / {b:6}: {v:7.2f} mm3")
    print(f"overlap panel      / bezel : {(panel & asm['bezel']).volume:7.2f} mm3")
    print(f"overlap esp32 pcb  / others: {sum((esp & asm[k]).volume for k in asm):7.2f} mm3")
    # The tongues must sit right under the top wall (touching, not overlapping)
    gap = asm["bezel"].distance_to(asm["body"])
    print(f"bezel-body distance: {gap:.3f} mm")

    # Print orientations
    print_pose = {
        "bezel": Rot(180, 0, 0) * bezel,       # face down
        "backer": backer,
        "body": Rot(0, -90, 0) * body,         # on its left side: no bridges, only the ears need support
        "base": base,
        "button_cap": Rot(180, 0, 0) * cap,    # top down
    }
    for name, part in print_pose.items():
        export_stl(part, os.path.join(HERE, "stl", f"{name}.stl"))
        export_step(asm[name], os.path.join(HERE, "step", f"{name}.step"))
        bb = part.bounding_box()
        print(f"{name:10} {bb.size.X:6.1f} x {bb.size.Y:6.1f} x {bb.size.Z:6.1f} mm  {part.volume / 1000:5.1f} cm3")

    # Drawings: assembled front, assembled back, exploded
    whole = asm["bezel"] + asm["backer"] + asm["body"] + asm["base"] + asm["button_cap"]
    n_out = (0, -c * 1.0, s * 1.0)
    exploded = (Pos(n_out[0] * 45, n_out[1] * 45, n_out[2] * 45) * asm["bezel"]
                + Pos(n_out[0] * 25, n_out[1] * 25, n_out[2] * 25) * asm["backer"]
                + asm["body"] + Pos(0, 0, -35) * asm["base"] + Pos(0, 0, 25) * asm["button_cap"])
    views = {
        "assembly_front": (whole, (-230, -330, 200), (0, 40, 30)),
        "assembly_back": (whole, (260, 380, 160), (0, 40, 20)),
        "exploded": (exploded, (-300, -300, 260), (0, 20, 10)),
    }
    for name, (shape, eye, target) in views.items():
        visible, _hidden = shape.project_to_viewport(eye, viewport_up=(0, 0, 1), look_at=target)
        svg = ExportSVG(scale=1.0, line_weight=0.3)
        svg.add_layer("visible", line_weight=0.35)
        svg.add_shape(visible, layer="visible")
        svg.write(os.path.join(HERE, f"{name}.svg"))

if __name__ == "__main__":
    main()
