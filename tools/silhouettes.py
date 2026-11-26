"""Technical side-profile aircraft illustrations (nose to the right).

Every family in data/families.csv has a builder here. Shapes are described in
"design units" (fuselage length ~100, y pointing up), then fitted and rasterised
to 1-bit bitmaps by gen_assets.py (232x80 small, 416x140 large).

Each component is drawn back-to-front with opaque background fills and crisp
scale-aware ink strokes (aviation manual / blueprint style) so overlapping
3D geometry stays sharp in both light and dark e-paper themes.
"""
import math

# ----------------------------------------------------------------------------
# Primitive helpers (all return lists of (x, y) points, y up)


def ellipse(cx, cy, rx, ry, a0=0.0, a1=360.0, n=36):
    return [
        (cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
         cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n)))
        for i in range(n + 1)
    ]


def quad_bezier(p0, p1, p2, n=16):
    return [
        ((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
         (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])
        for t in (i / n for i in range(n + 1))
    ]


def cubic_bezier(p0, p1, p2, p3, n=20):
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        x = u**3 * p0[0] + 3 * u**2 * t * p1[0] + 3 * u * t**2 * p2[0] + t**3 * p3[0]
        y = u**3 * p0[1] + 3 * u**2 * t * p1[1] + 3 * u * t**2 * p2[1] + t**3 * p3[1]
        pts.append((x, y))
    return pts


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def rounded_rect(x0, y0, x1, y1, r, n=4):
    r = min(r, abs(x1 - x0) / 2, abs(y1 - y0) / 2)
    pts = []
    pts += ellipse(x1 - r, y1 - r, r, r, 0, 90, n)
    pts += ellipse(x0 + r, y1 - r, r, r, 90, 180, n)
    pts += ellipse(x0 + r, y0 + r, r, r, 180, 270, n)
    pts += ellipse(x1 - r, y0 + r, r, r, 270, 360, n)
    return pts


class Sil:
    """Vector drawing recorder supporting technical outlines & legacy ops."""

    def __init__(self):
        self.ops = []

    def poly(self, pts, fill=1, sep=0.0):
        self.ops.append(("poly", pts, fill, sep))
        return self

    def line(self, pts, width, fill=1):
        self.ops.append(("line", pts, fill, width))
        return self

    def white(self, pts):
        return self.poly(pts, fill=0)

    def shape(self, pts, fill=0, stroke=1, width=1.5, closed=True, min_w=0):
        """fill/stroke in (None, 0=paper, 1=ink). width is reference px at 232x80."""
        self.ops.append(("shape", pts, fill, stroke, width, closed, min_w))
        return self

    def solid(self, pts, min_w=0):
        return self.shape(pts, fill=1, stroke=None, width=0, closed=True, min_w=min_w)

    def panel(self, pts, fill=0, width=1.5, min_w=0):
        return self.shape(pts, fill=fill, stroke=1, width=width, closed=True, min_w=min_w)

    def stroke(self, pts, width=1.0, closed=False, color=1, min_w=0):
        return self.shape(pts, fill=None, stroke=color, width=width, closed=closed, min_w=min_w)


# ----------------------------------------------------------------------------
# Shared technical aviation drawing helpers


def draw_window_row(s, x_start, x_end, y, w=0.9, h=1.05, step=2.15, gaps=(), oval=False):
    x = x_end
    while x >= x_start:
        if not any(g0 <= x <= g1 for g0, g1 in gaps):
            if oval:
                s.solid(ellipse(x, y, w / 2, h / 2, n=12))
            else:
                s.solid(rounded_rect(x - w / 2, y - h / 2, x + w / 2, y + h / 2, r=w * 0.35, n=4))
        x -= step


def draw_door(s, xc, y_bot, y_top, w=1.8, min_w=0):
    s.stroke(rounded_rect(xc - w / 2, y_bot, xc + w / 2, y_top, r=w * 0.25, n=3),
             width=0.88, closed=True, min_w=min_w)
    wy = y_top - (y_top - y_bot) * 0.30
    s.solid(rect(xc - 0.35, wy - 0.38, xc + 0.35, wy + 0.38), min_w=300)


def draw_turbofan(s, x_front, length, yc, h, pylon_pts=None, far=False, chevron=False, slim=False):
    """Technical turbofan pod: pylon, core nozzle + exhaust cone, fan cowl, intake lip."""
    r = h / 2.0
    if pylon_pts:
        s.panel(pylon_pts, fill=0, width=1.25 if not far else 1.15)

    cowl_len = length * (0.76 if slim else 0.72)
    nozzle_r = r * (0.68 if slim else 0.60)
    cone_x = x_front - length * 1.07

    core_pts = [
        (x_front - cowl_len * 0.86, yc + r * 0.74),
        (x_front - length, yc + nozzle_r * 0.80),
        (x_front - length, yc - nozzle_r * 0.80),
        (x_front - cowl_len * 0.86, yc - r * 0.74),
    ]
    s.panel(core_pts, fill=0, width=1.2 if not far else 1.1)
    s.panel([
        (x_front - length, yc + nozzle_r * 0.40),
        (cone_x, yc),
        (x_front - length, yc - nozzle_r * 0.40),
    ], fill=1 if not far else 0, width=1.0)

    lip_r = r * 0.38
    cowl = []
    cowl += cubic_bezier((x_front, yc), (x_front, yc + r * 0.82),
                         (x_front - cowl_len * 0.22, yc + r), (x_front - cowl_len * 0.48, yc + r), n=12)
    cowl += cubic_bezier((x_front - cowl_len * 0.48, yc + r), (x_front - cowl_len * 0.78, yc + r * 0.98),
                         (x_front - cowl_len * 0.94, yc + r * 0.86), (x_front - cowl_len, yc + r * 0.78), n=8)
    if chevron:
        # Signature serrated chevron trailing edge (787 / 737 MAX)
        cx0 = x_front - cowl_len
        for k in range(3):
            y_hi = yc + r * 0.78 - k * (r * 1.56 / 3)
            y_mid = y_hi - r * 1.56 / 6
            y_lo = y_hi - r * 1.56 / 3
            cowl.append((cx0 - 0.8, y_mid))
            cowl.append((cx0, y_lo))
    else:
        cowl.append((x_front - cowl_len, yc - r * 0.78))
    cowl += cubic_bezier((x_front - cowl_len, yc - r * 0.78), (x_front - cowl_len * 0.92, yc - r * 0.88),
                         (x_front - cowl_len * 0.72, yc - r * 0.96), (x_front - cowl_len * 0.45, yc - r * 0.96), n=8)
    cowl += cubic_bezier((x_front - cowl_len * 0.45, yc - r * 0.96), (x_front - cowl_len * 0.18, yc - r * 0.94),
                         (x_front, yc - r * 0.78), (x_front, yc), n=12)
    s.panel(cowl, fill=0, width=1.45 if not far else 1.25)

    lip_x = x_front - lip_r * 1.15
    s.stroke(cubic_bezier((lip_x, yc + r * 0.88), (lip_x - 0.5, yc + r * 0.3),
                          (lip_x - 0.5, yc - r * 0.3), (lip_x, yc - r * 0.85), n=8), width=0.95)
    if not far:
        tr_x = x_front - cowl_len * 0.62
        s.stroke([(tr_x, yc + r * 0.95), (tr_x, yc - r * 0.92)], width=0.85, min_w=300)
        s.solid(ellipse(x_front - 0.45, yc, 0.45, r * 0.62, n=12))


def draw_turboprop(s, x_front, length, yc, h, prop_r, far=False, intake_below=True):
    """Technical turboprop engine nacelle: tapered cowling, intake scoop, spinner & prop disc."""
    r = h / 2.0
    # Sleek tapered turboprop nacelle
    nac = (
        cubic_bezier((x_front, yc + r * 0.55), (x_front - length * 0.15, yc + r),
                     (x_front - length * 0.55, yc + r * 0.95), (x_front - length, yc + r * 0.25), n=10)
        + [(x_front - length, yc - r * 0.15)]
        + cubic_bezier((x_front - length, yc - r * 0.15), (x_front - length * 0.55, yc - r * 0.85),
                       (x_front - length * 0.15, yc - r * 0.95), (x_front, yc - r * 0.55), n=10)
    )
    s.panel(nac, fill=0, width=1.35 if not far else 1.2)
    # Chin air intake scoop
    if intake_below and not far:
        s.panel([(x_front - 0.6, yc - r * 0.55), (x_front - 0.6, yc - r * 1.15),
                 (x_front - length * 0.38, yc - r * 0.95), (x_front - length * 0.38, yc - r * 0.55)],
                fill=0, width=1.0)
        s.solid([(x_front - 0.7, yc - r * 0.62), (x_front - 0.7, yc - r * 1.08),
                 (x_front - 1.5, yc - r * 1.04), (x_front - 1.5, yc - r * 0.62)])
    # Exhaust pipe stub detail
    if not far:
        s.stroke([(x_front - length * 0.55, yc + r * 0.8), (x_front - length * 0.55, yc - r * 0.7)],
                 width=0.85, min_w=300)
    # Vertical tapered propeller blade disc
    px = x_front + 0.9
    blade = [
        (px - 0.35, yc - prop_r), (px + 0.35, yc - prop_r),
        (px + 0.65, yc), (px + 0.35, yc + prop_r),
        (px - 0.35, yc + prop_r), (px - 0.65, yc),
    ]
    s.panel(blade, fill=1, width=0.85)
    # Conical spinner hub
    spin = [
        (x_front, yc + r * 0.55),
        (x_front + 1.5, yc + r * 0.40),
        (x_front + 3.2, yc),
        (x_front + 1.5, yc - r * 0.40),
        (x_front, yc - r * 0.55),
    ]
    s.panel(spin, fill=0, width=1.15 if not far else 1.05)


def draw_wing_root(s, x_le, x_te, y_root, thick=2.1, fairing_le=None, fairing_te=None, belly_drop=1.0):
    if fairing_le is not None and fairing_te is not None:
        f_pts = cubic_bezier((fairing_le, 0.0), (fairing_le - 2.5, -belly_drop * 1.05),
                             (fairing_te + 3.0, -belly_drop * 1.05), (fairing_te, 0.0), n=16)
        s.panel(f_pts + [(fairing_te, 1.5), (fairing_le, 1.5)], fill=0, width=0.0)
        s.stroke(f_pts, width=1.45)

    chord = x_le - x_te
    airfoil = (
        cubic_bezier((x_le, y_root), (x_le - chord * 0.12, y_root + thick * 0.95),
                     (x_le - chord * 0.45, y_root + thick * 0.85), (x_te, y_root + thick * 0.08), n=14)
        + cubic_bezier((x_te, y_root - thick * 0.05), (x_le - chord * 0.50, y_root - thick * 0.25),
                       (x_le - chord * 0.14, y_root - thick * 0.55), (x_le, y_root), n=14)
    )
    s.panel(airfoil, fill=0, width=1.2)


def draw_horiz_stab(s, x_le, x_te, y, thick=1.8, elevator=True):
    chord = x_le - x_te
    pts = [
        (x_le, y + thick * 0.10),
        (x_le - chord * 0.28, y + thick * 0.55),
        (x_te, y + thick * 0.18),
        (x_te, y - thick * 0.18),
        (x_le - chord * 0.30, y - thick * 0.45),
    ]
    s.panel(pts, fill=0, width=1.35)
    if elevator:
        ex = x_te + chord * 0.32
        s.stroke([(ex, y + thick * 0.32), (ex, y - thick * 0.28)], width=0.85, min_w=300)


def draw_high_wing_box(s, x_le, x_te, y_top, thick=2.4):
    """High wing root airfoil & dorsal wingbox fairing."""
    chord = x_le - x_te
    pts = (
        cubic_bezier((x_le, y_top), (x_le - chord * 0.12, y_top + thick),
                     (x_le - chord * 0.48, y_top + thick * 0.92), (x_te, y_top + thick * 0.12), n=12)
        + cubic_bezier((x_te, y_top - thick * 0.15), (x_le - chord * 0.52, y_top - thick * 0.30),
                       (x_le - chord * 0.14, y_top - thick * 0.45), (x_le, y_top), n=12)
    )
    s.panel(pts, fill=0, width=1.35)


def draw_cockpit_2pane(s, L, h, dx=3.3, y0=0.56, y1=0.76, w1=3.0, w2=2.0, slant=0.9):
    """Crisp 2-pane airline/transport cockpit glazing."""
    s.solid([
        (L - dx, h * y0),
        (L - dx - slant, h * y1),
        (L - dx - w1, h * y1),
        (L - dx - w1, h * y0),
    ])
    g = 0.5
    s.solid([
        (L - dx - w1 - g, h * y0),
        (L - dx - w1 - g, h * y1),
        (L - dx - w1 - g - w2, h * (y1 - 0.01)),
        (L - dx - w1 - g - w2 - 0.25, h * (y0 + 0.02)),
    ])


def draw_fixed_wheel(s, x, y_wheel, y_top, r=3.0, pant=False, strut_dx=0.0):
    """Fixed landing gear strut + wheel (optionally with aerodynamic wheel pant)."""
    s.stroke([(x + strut_dx, y_top), (x, y_wheel)], width=1.3)
    if pant:
        pts = (
            cubic_bezier((x + r * 1.5, y_wheel - r * 0.1), (x + r * 1.4, y_wheel + r * 1.05),
                         (x - r * 0.4, y_wheel + r * 1.05), (x - r * 2.1, y_wheel - r * 0.15), n=12)
            + cubic_bezier((x - r * 2.1, y_wheel - r * 0.15), (x - r * 0.5, y_wheel - r * 0.75),
                           (x + r * 1.2, y_wheel - r * 0.75), (x + r * 1.5, y_wheel - r * 0.1), n=10)
        )
        s.panel(pts, fill=0, width=1.25)
        s.solid(ellipse(x, y_wheel - r * 0.55, r * 0.55, r * 0.45, n=10))
    else:
        s.panel(ellipse(x, y_wheel, r, r, n=16), fill=1, width=0.8)
        s.solid(ellipse(x, y_wheel, r * 0.42, r * 0.42, n=10))
        s.shape(ellipse(x, y_wheel, r * 0.42, r * 0.42, n=10), fill=0, stroke=None, width=0)


# ----------------------------------------------------------------------------
# Versatile technical airliner builder (unified fuselage + fin contour)


def tech_airliner(
    h=10.4,
    fin_top=26.5,
    nose="airbus",          # airbus | boeing | sleek | a350 | douglas
    tail="standard",        # standard | chisel | pointed
    t_tail=False,
    cruciform_y=None,
    dorsal_kink=False,      # Boeing 737 iconic dorsal fin knee
    fin_root_x=25.5,
    fin_le_tip_x=10.0,
    fin_te_tip_x=4.2,
    fin_te_base_x=6.2,
    engines="under2",       # under2 | under4 | rear2 | rear3 | tail3 | none
    eng_x=64.5,
    eng_len=12.5,
    eng_h=5.5,
    eng_yc=-2.4,
    eng_chevron=False,
    eng_slim=False,
    far_eng_x=50.5,
    far_eng_scale=0.88,
    wing_le=63.0,
    wing_te=41.5,
    belly_drop=1.3,
    win_start=25.5,
    win_end=83.0,
    win_y=0.60,
    win_w=0.90,
    win_h=1.08,
    win_step=2.15,
    win_oval=False,
    doors=None,
    overwing_exits=None,
    tail_upsweep_x=30.0,
    s_duct=False,
    split_airbrake=False,
    extra_fn=None,
):
    s = Sil()
    L = 100.0

    # 1. Far outboard engine (for 4-engine jets)
    if engines == "under4":
        fl = eng_len * far_eng_scale
        fh = eng_h * far_eng_scale
        fyc = eng_yc - 0.9
        draw_turbofan(
            s, x_front=far_eng_x, length=fl, yc=fyc, h=fh,
            pylon_pts=[(far_eng_x - 2.2, 0.6), (far_eng_x - 6.0, 0.6),
                       (far_eng_x - 7.5, fyc + fh * 0.35), (far_eng_x - 3.5, fyc + fh * 0.35)],
            far=True, chevron=eng_chevron, slim=eng_slim,
        )

    # 2. Unified fuselage + dorsal fillet + vertical stabilizer contour
    body = []
    if nose == "airbus":
        body += cubic_bezier((L, h * 0.38), (L, h * 0.74), (L - 3.0, h * 0.98), (L - 8.2, h), n=16)
        nose_lo = cubic_bezier((L - 8.5, 0.0), (L - 3.4, 0.0), (L, h * 0.10), (L, h * 0.38), n=14)
        radome_x = L - 3.6
    elif nose == "boeing":
        body += cubic_bezier((L, h * 0.32), (L - 0.6, h * 0.62), (L - 4.8, h * 0.95), (L - 10.5, h), n=16)
        nose_lo = cubic_bezier((L - 10.0, 0.0), (L - 4.2, 0.0), (L - 0.5, h * 0.08), (L, h * 0.32), n=14)
        radome_x = L - 4.2
    elif nose == "sleek":
        # 787 / A220 / modern continuous curved nose
        body += cubic_bezier((L, h * 0.34), (L - 1.0, h * 0.66), (L - 5.2, h * 0.96), (L - 11.5, h), n=16)
        nose_lo = cubic_bezier((L - 10.5, 0.0), (L - 4.2, 0.0), (L - 0.6, h * 0.10), (L, h * 0.34), n=14)
        radome_x = L - 4.0
    elif nose == "a350":
        body += cubic_bezier((L, h * 0.36), (L - 0.6, h * 0.68), (L - 4.2, h * 0.96), (L - 10.2, h), n=16)
        nose_lo = cubic_bezier((L - 9.5, 0.0), (L - 3.8, 0.0), (L - 0.4, h * 0.09), (L, h * 0.36), n=14)
        radome_x = L - 3.8
    elif nose == "douglas":
        # Sleek pointed DC-9 / MD-80 / ERJ / CRJ / Bizjet nose
        body += cubic_bezier((L, h * 0.34), (L - 1.8, h * 0.60), (L - 6.8, h * 0.94), (L - 13.5, h), n=16)
        nose_lo = cubic_bezier((L - 12.0, 0.0), (L - 5.2, 0.0), (L - 1.2, h * 0.08), (L, h * 0.34), n=14)
        radome_x = L - 4.6
    else:
        raise ValueError(nose)

    # Dorsal spine & vertical fin transition
    if dorsal_kink:
        # Signature Boeing 737 forward dorsal fin extension!
        body.append((fin_root_x + 5.5, h))
        body.append((fin_root_x - 2.5, h * 1.26))
        body.append((fin_le_tip_x, fin_top - 0.5))
    elif s_duct:
        # Boeing 727 dorsal S-duct intake scoop at fin root
        body.append((fin_root_x + 3.5, h))
        body.append((fin_root_x + 3.5, h * 1.38))
        body.append((fin_root_x - 0.5, h * 1.38))
        body.append((fin_le_tip_x, fin_top - 0.5))
    else:
        body.append((fin_root_x, h))
        body += cubic_bezier(
            (fin_root_x, h),
            (fin_root_x - 3.5, h * 1.03),
            (fin_root_x - 6.0, h * 1.16),
            (fin_root_x - 8.2, h * 1.44),
            n=12,
        )
        body.append((fin_le_tip_x, fin_top - 0.5))

    # Fin tip
    body += quad_bezier((fin_le_tip_x, fin_top - 0.5), (fin_le_tip_x - 0.6, fin_top),
                        (fin_le_tip_x - 1.6, fin_top), n=5)
    body.append((fin_te_tip_x, fin_top))
    body += quad_bezier((fin_te_tip_x, fin_top), (fin_te_tip_x - 0.6, fin_top),
                        (fin_te_tip_x - 0.7, fin_top - 0.5), n=5)
    body.append((fin_te_base_x, h * 0.85))

    # Tailcone profile
    if tail == "chisel":
        # Boeing 777 signature blade-like flat APU tailcone
        body += cubic_bezier((fin_te_base_x, h * 0.85), (3.2, h * 0.80), (1.0, h * 0.76), (0.0, h * 0.73), n=8)
        body.append((0.0, h * 0.42))
        body += cubic_bezier((0.0, h * 0.42), (9.0, h * 0.24), (19.0, h * 0.04), (tail_upsweep_x, 0.0), n=14)
    elif tail == "pointed":
        body += cubic_bezier((fin_te_base_x, h * 0.85), (3.5, h * 0.78), (1.2, h * 0.70), (0.0, h * 0.64), n=8)
        body.append((0.0, h * 0.54))
        body += cubic_bezier((0.0, h * 0.54), (9.5, h * 0.28), (19.5, h * 0.04), (tail_upsweep_x, 0.0), n=14)
    else:
        body += cubic_bezier((fin_te_base_x, h * 0.85), (3.5, h * 0.80), (1.2, h * 0.76), (0.0, h * 0.72), n=8)
        body.append((0.0, h * 0.60))
        body += cubic_bezier((0.0, h * 0.60), (9.5, h * 0.29), (19.5, h * 0.04), (tail_upsweep_x, 0.0), n=14)

    body.append(nose_lo[0])
    body += nose_lo
    s.panel(body, fill=0, width=1.6)

    # Rudder hinge line
    rud_top_x = fin_te_tip_x + (fin_le_tip_x - fin_te_tip_x) * 0.28
    rud_bot_x = fin_te_base_x + 2.0
    s.stroke([(rud_top_x, fin_top), (rud_bot_x, h * 0.85)], width=0.95)
    # Subtle fin-root crease on large view
    if not dorsal_kink and not s_duct:
        s.stroke([(fin_root_x - 4.5, h), (rud_bot_x, h * 0.89)], width=0.8, min_w=300)

    # Radome seam
    s.stroke(cubic_bezier((radome_x, h * 0.74), (radome_x - 0.5, h * 0.44),
                          (radome_x - 0.5, h * 0.24), (radome_x, h * 0.08), n=8), width=0.85)

    # S-duct dark intake mouth on B727
    if s_duct:
        s.solid([(fin_root_x + 3.4, h * 1.33), (fin_root_x + 2.6, h * 1.33),
                 (fin_root_x + 2.6, h * 1.04), (fin_root_x + 3.4, h * 1.04)])
        # #2 center exhaust nozzle at tail tip
        s.panel([(1.5, h * 0.76), (-1.8, h * 0.72), (-1.8, h * 0.52), (1.5, h * 0.48)], fill=0, width=1.15)
        s.solid([(-0.6, h * 0.68), (-1.8, h * 0.67), (-1.8, h * 0.57), (-0.6, h * 0.56)])

    if split_airbrake:
        s.stroke([(4.5, h * 0.74), (4.5, h * 0.52)], width=0.9)
        s.stroke([(4.5, h * 0.63), (0.2, h * 0.63)], width=0.85)

    # 3. Cockpit glazing (tailored per nose style)
    if nose == "a350":
        # Signature A350 curved "raccoon mask" wrap-around windscreen
        s.solid(
            cubic_bezier((L - 3.6, h * 0.56), (L - 4.6, h * 0.78), (L - 6.8, h * 0.80), (L - 10.6, h * 0.74), n=10)
            + [(L - 10.8, h * 0.58), (L - 3.6, h * 0.56)]
        )
    elif nose == "sleek":
        draw_cockpit_2pane(s, L, h, dx=4.2, y0=0.56, y1=0.76, w1=3.2, w2=2.2, slant=1.2)
    elif nose == "boeing":
        draw_cockpit_2pane(s, L, h, dx=4.4, y0=0.56, y1=0.77, w1=3.0, w2=2.1, slant=1.1)
    elif nose == "douglas":
        draw_cockpit_2pane(s, L, h, dx=5.4, y0=0.55, y1=0.76, w1=3.2, w2=2.1, slant=1.3)
    else:
        draw_cockpit_2pane(s, L, h, dx=3.3, y0=0.56, y1=0.76, w1=3.0, w2=1.9, slant=0.9)

    # 4. Doors & cabin windows
    gaps = []
    if doors:
        for dx, big_only in doors:
            draw_door(s, dx, h * 0.23, h * 0.78, w=1.8, min_w=300 if big_only else 0)
            gaps.append((dx - 1.5, dx + 1.5))
    if overwing_exits:
        for dx in overwing_exits:
            draw_door(s, dx, h * 0.36, h * 0.74, w=1.4, min_w=300)
            gaps.append((dx - 1.2, dx + 1.2))
    draw_window_row(s, win_start, win_end, h * win_y, w=win_w, h=win_h, step=win_step, gaps=gaps, oval=win_oval)

    # 5. Horizontal stabilizer (low, cruciform, or T-tail with bullet fairing)
    if t_tail:
        bullet_y = fin_top - 0.9
        # T-tail bullet fairing at fin tip
        s.panel(ellipse(fin_le_tip_x - 2.6, bullet_y, 5.2, 1.35, n=16), fill=0, width=1.2)
        draw_horiz_stab(s, fin_le_tip_x + 0.8, fin_te_tip_x - 7.5, bullet_y, thick=1.7)
    elif cruciform_y is not None:
        cy = cruciform_y
        # Interpolate fin x at cy
        t_frac = (cy - h) / max(1.0, fin_top - h)
        cx_le = fin_root_x - 4.0 + (fin_le_tip_x - (fin_root_x - 4.0)) * t_frac
        cx_te = fin_te_base_x + (fin_te_tip_x - fin_te_base_x) * t_frac
        draw_horiz_stab(s, cx_le + 2.0, cx_te - 7.5, cy, thick=1.7)
    else:
        draw_horiz_stab(s, 15.2, -1.6, h * 0.65, thick=1.8)

    # 6. Wing-to-body fairing + wing root airfoil
    draw_wing_root(s, x_le=wing_le, x_te=wing_te, y_root=h * 0.14, thick=2.0,
                   fairing_le=wing_le + 5.0, fairing_te=wing_te - 4.5, belly_drop=belly_drop)

    # 7. Engines
    if engines in ("under2", "under4"):
        pylon = [(eng_x - 2.5, 1.5), (eng_x - 8.2, 2.1),
                 (eng_x - 12.2, eng_yc + eng_h * 0.25), (eng_x - 6.8, eng_yc + eng_h * 0.25)]
        draw_turbofan(s, x_front=eng_x, length=eng_len, yc=eng_yc, h=eng_h,
                      pylon_pts=pylon, chevron=eng_chevron, slim=eng_slim)
    elif engines in ("rear2", "rear3"):
        rx = eng_x
        ryc = h * 0.86
        draw_turbofan(s, x_front=rx, length=eng_len, yc=ryc, h=eng_h,
                      pylon_pts=None, chevron=eng_chevron, slim=eng_slim)
    elif engines == "tail3":
        # DC-10 / MD-11: underwing turbofan + straight-through #2 tail-mounted turbofan!
        pylon = [(eng_x - 2.5, 1.5), (eng_x - 8.0, 2.0),
                 (eng_x - 11.8, eng_yc + eng_h * 0.25), (eng_x - 6.8, eng_yc + eng_h * 0.25)]
        draw_turbofan(s, x_front=eng_x, length=eng_len, yc=eng_yc, h=eng_h, pylon_pts=pylon)
        # #2 engine pod embedded through the lower vertical stabilizer!
        draw_turbofan(s, x_front=fin_root_x - 1.5, length=eng_len * 1.45,
                      yc=h + eng_h * 0.50, h=eng_h * 0.90, pylon_pts=None, slim=True)

    if extra_fn:
        extra_fn(s, h)

    return s


# ----------------------------------------------------------------------------
# Versatile high-wing transport / regional turboprop builder


def tech_high_winger(
    h=11.4,
    fin_top=27.5,
    nose="round",           # round | pointed | visor | glazed | blunt
    t_tail=True,
    h_tail=False,           # An-225 twin endplate tail!
    fin_root_x=25.5,
    fin_le_tip_x=11.0,
    fin_te_tip_x=4.5,
    fin_te_base_x=6.5,
    engines="tp2",          # tp2 | tp4 | jet4 | jet6
    eng_x=64.0,
    eng_len=14.0,
    eng_h=4.6,
    prop_r=7.8,
    upsweep=True,
    sponson=True,
    sponson_x=(40.0, 68.0),
    wing_le=66.5,
    wing_te=45.0,
    wins=False,
    portholes=False,
    doors=None,
    probe=False,
    airbrake=False,
    extra_fn=None,
):
    s = Sil()
    L = 100.0

    # 1. Far outboard engine(s) behind fuselage/wing (for jet4 / jet6 under-belly visibility)
    if engines == "jet4":
        fyc = -0.6
        draw_turbofan(s, x_front=eng_x - 12.5, length=eng_len * 0.88, yc=fyc, h=eng_h * 0.88,
                      pylon_pts=[(eng_x - 15.0, 1.0), (eng_x - 19.0, 1.0),
                                 (eng_x - 19.5, fyc + eng_h * 0.2), (eng_x - 15.8, fyc + eng_h * 0.2)], far=True)
    elif engines == "jet6":
        # An-225: 3 engines per wing — clearly staggered inboard, mid & outboard!
        draw_turbofan(s, x_front=eng_x - 20.0, length=eng_len * 0.80, yc=-1.6, h=eng_h * 0.80,
                      pylon_pts=[(eng_x - 22.0, 0.8), (eng_x - 25.0, 0.8),
                                 (eng_x - 25.5, -0.6), (eng_x - 22.5, -0.6)], far=True)
        draw_turbofan(s, x_front=eng_x - 10.5, length=eng_len * 0.88, yc=-0.8, h=eng_h * 0.88,
                      pylon_pts=[(eng_x - 12.5, 1.0), (eng_x - 16.0, 1.0),
                                 (eng_x - 16.5, 0.2), (eng_x - 13.0, 0.2)], far=True)

    # Far H-tail vertical fin (An-225)
    if h_tail:
        s.panel([(12.5, h * 0.55), (5.5, h * 0.55), (1.0, fin_top * 0.95), (6.5, fin_top * 0.95)],
                fill=0, width=1.2)

    # 2. Unified fuselage + vertical stabilizer (unless h_tail has no center fin)
    body = []
    if nose == "pointed":
        body += cubic_bezier((L, h * 0.34), (L - 1.2, h * 0.64), (L - 5.5, h * 0.94), (L - 11.5, h), n=16)
        nose_lo = cubic_bezier((L - 10.5, 0.0), (L - 4.2, 0.0), (L - 0.8, h * 0.08), (L, h * 0.34), n=14)
        radome_x = L - 4.4
    elif nose == "visor":
        # Drooping heavy cargo snout (C-5 / An-124 / An-225)
        body += cubic_bezier((L, h * 0.44), (L - 0.4, h * 0.72), (L - 3.6, h * 0.96), (L - 10.5, h), n=16)
        nose_lo = cubic_bezier((L - 11.0, 0.0), (L - 4.2, h * 0.02), (L, h * 0.18), (L, h * 0.44), n=14)
        radome_x = L - 4.0
    elif nose == "glazed":
        # Il-76 rounded navigator nose
        body += cubic_bezier((L, h * 0.46), (L, h * 0.78), (L - 2.8, h * 0.98), (L - 8.5, h), n=16)
        nose_lo = cubic_bezier((L - 9.0, 0.0), (L - 3.0, h * 0.02), (L, h * 0.18), (L, h * 0.46), n=14)
        radome_x = L - 3.5
    elif nose == "blunt":
        body += cubic_bezier((L, h * 0.38), (L, h * 0.76), (L - 2.6, h * 0.98), (L - 7.8, h), n=16)
        nose_lo = cubic_bezier((L - 8.0, 0.0), (L - 2.8, 0.0), (L, h * 0.10), (L, h * 0.38), n=14)
        radome_x = L - 3.4
    else:
        body += cubic_bezier((L, h * 0.40), (L, h * 0.75), (L - 3.2, h * 0.97), (L - 8.8, h), n=16)
        nose_lo = cubic_bezier((L - 9.0, 0.0), (L - 3.5, 0.0), (L, h * 0.12), (L, h * 0.40), n=14)
        radome_x = L - 3.6

    if h_tail:
        # An-225: no central fin — spine flows straight back to horizontal tailplane mount
        body.append((10.0, h * 0.94))
        body += cubic_bezier((10.0, h * 0.94), (4.5, h * 0.88), (1.5, h * 0.82), (0.0, h * 0.76), n=8)
        body.append((0.0, h * 0.62))
    else:
        body.append((fin_root_x, h))
        body += cubic_bezier(
            (fin_root_x, h),
            (fin_root_x - 3.5, h * 1.04),
            (fin_root_x - 6.0, h * 1.18),
            (fin_root_x - 8.0, h * 1.44),
            n=12,
        )
        body.append((fin_le_tip_x, fin_top - 0.5))
        body += quad_bezier((fin_le_tip_x, fin_top - 0.5), (fin_le_tip_x - 0.6, fin_top),
                            (fin_le_tip_x - 1.6, fin_top), n=5)
        body.append((fin_te_tip_x, fin_top))
        body += quad_bezier((fin_te_tip_x, fin_top), (fin_te_tip_x - 0.6, fin_top),
                            (fin_te_tip_x - 0.7, fin_top - 0.5), n=5)
        body.append((fin_te_base_x, h * 0.88))
        body += cubic_bezier((fin_te_base_x, h * 0.88), (3.5, h * 0.84), (1.2, h * 0.80), (0.0, h * 0.75), n=8)
        body.append((0.0, h * 0.64))

    if upsweep:
        # High tactical rear cargo ramp upsweep
        body += cubic_bezier((0.0, h * 0.64), (11.5, h * 0.44), (24.5, h * 0.06), (37.0, 0.0), n=14)
    else:
        body += cubic_bezier((0.0, h * 0.64), (9.5, h * 0.30), (19.5, h * 0.04), (30.0, 0.0), n=14)

    body.append(nose_lo[0])
    body += nose_lo
    s.panel(body, fill=0, width=1.6)

    if not h_tail:
        rud_top_x = fin_te_tip_x + (fin_le_tip_x - fin_te_tip_x) * 0.28
        s.stroke([(rud_top_x, fin_top), (fin_te_base_x + 2.0, h * 0.88)], width=0.95)
    if upsweep:
        # Rear cargo loading ramp door seam
        s.stroke([(33.0, h * 0.06), (25.5, h * 0.46), (13.0, h * 0.60)], width=0.9)
    if airbrake:
        s.stroke([(4.5, h * 0.76), (4.5, h * 0.54)], width=0.9)
        s.stroke([(4.5, h * 0.65), (0.2, h * 0.65)], width=0.85)
    if nose == "visor":
        # Hinged upward-opening visor nose seam
        s.stroke([(L - 10.5, h * 0.65), (L - 12.0, 0.0)], width=0.9)
    else:
        s.stroke(cubic_bezier((radome_x, h * 0.76), (radome_x - 0.5, h * 0.48),
                              (radome_x - 0.5, h * 0.26), (radome_x, h * 0.10), n=8), width=0.85)

    if probe:
        # Aerial refueling probe above flight deck (A400M)
        s.stroke([(L - 11.0, h * 1.00), (L - 5.5, h * 1.15), (L - 2.0, h * 1.12)], width=1.0)

    # 3. Cockpit + optional glazed navigator nose / windows
    if nose == "visor":
        draw_cockpit_2pane(s, L, h, dx=6.2, y0=0.72, y1=0.88, w1=3.0, w2=2.0, slant=1.0)
    elif nose == "glazed":
        draw_cockpit_2pane(s, L, h, dx=4.0, y0=0.66, y1=0.84, w1=3.0, w2=2.0, slant=1.0)
        # Signature Il-76 navigator glazed nose panes
        for i in range(3):
            s.solid(rounded_rect(L - 2.8 - i * 1.65, h * 0.20, L - 1.9 - i * 1.65, h * 0.40, r=0.2, n=3))
    else:
        draw_cockpit_2pane(s, L, h, dx=3.8, y0=0.60, y1=0.79, w1=3.1, w2=2.1, slant=1.1)

    if doors:
        for dx, big_only in doors:
            draw_door(s, dx, h * 0.22, h * 0.74, w=1.8, min_w=300 if big_only else 0)
    if wins:
        gaps = [(dx - 1.5, dx + 1.5) for dx, _ in (doors or ())]
        draw_window_row(s, 29.0, L - 15.0, h * 0.56, w=0.92, h=1.08, step=2.3, gaps=gaps)
    elif portholes:
        for wx in (76.0, 68.0, 44.0, 36.0):
            s.solid(ellipse(wx, h * 0.56, 0.95, 0.95, n=10))

    # 4. Horizontal tail (T-tail, low tail, or An-225 H-tail)
    if h_tail:
        draw_horiz_stab(s, 14.5, 2.5, h * 0.72, thick=1.8)
        s.panel([(15.0, h * 0.48), (8.0, h * 0.48), (3.5, fin_top), (9.0, fin_top)], fill=0, width=1.5)
        s.stroke([(5.6, fin_top), (9.6, h * 0.50)], width=0.9)
    elif t_tail:
        bullet_y = fin_top - 0.9
        s.panel(ellipse(fin_le_tip_x - 2.4, bullet_y, 5.2, 1.35, n=16), fill=0, width=1.2)
        draw_horiz_stab(s, fin_le_tip_x + 1.2, fin_te_tip_x - 7.8, bullet_y, thick=1.8)
    else:
        draw_horiz_stab(s, 15.5, -1.6, h * 0.70, thick=1.9)

    # 5. High wing root + landing gear sponson
    draw_high_wing_box(s, wing_le, wing_te, h * 0.98, thick=2.4)
    if sponson:
        sx0, sx1 = sponson_x
        sp = (
            cubic_bezier((sx1, 0.2), (sx1 - 3.0, 3.6), (sx0 + 4.0, 3.6), (sx0, 0.6), n=14)
            + cubic_bezier((sx0, 0.6), (sx0 + 3.5, -1.6), (sx1 - 3.5, -1.6), (sx1, 0.2), n=14)
        )
        s.panel(sp, fill=0, width=1.35)
        s.stroke([(sx0 + (sx1 - sx0) * 0.5, 2.8), (sx0 + (sx1 - sx0) * 0.5, -1.0)], width=0.85, min_w=300)

    # 6. Near engine(s)
    if engines == "tp2":
        draw_turboprop(s, x_front=eng_x, length=eng_len, yc=h * 0.70, h=eng_h, prop_r=prop_r, far=False)
    elif engines == "tp4":
        # Both near-wing engines visible on the viewer's side: outboard slightly aft, then inboard in front!
        draw_turboprop(s, x_front=eng_x - 10.5, length=eng_len * 0.88, yc=h * 0.74,
                       h=eng_h * 0.88, prop_r=prop_r * 0.90, far=True)
        draw_turboprop(s, x_front=eng_x, length=eng_len, yc=h * 0.68, h=eng_h, prop_r=prop_r, far=False)
    elif engines in ("jet4", "jet6"):
        # High-wing jet pod slung forward beneath the swept wing with sleek angled pylon
        nyc = h * 0.04
        pylon = [(eng_x - 4.2, h * 0.88), (eng_x - 7.8, h * 0.88),
                 (eng_x - 6.8, nyc + eng_h * 0.35), (eng_x - 3.4, nyc + eng_h * 0.35)]
        draw_turbofan(s, x_front=eng_x, length=eng_len, yc=nyc, h=eng_h, pylon_pts=pylon, far=False)

    if extra_fn:
        extra_fn(s, h)

    return s


# ============================================================================
# ALL 64 FAMILY BUILDERS
# ============================================================================

# ---- Commercial Underwing Narrowbodies & Widebodies ------------------------


def b_generic_jet():
    return tech_airliner(
        h=10.4, fin_top=26.2, nose="airbus", eng_x=64.0, eng_len=12.2, eng_h=5.3,
        doors=[(86.0, False), (23.5, False)], overwing_exits=[54.0],
    )


def b_a220():
    # Airbus A220: sleek conical nose, large GTF turbofans, larger rectangular windows
    return tech_airliner(
        h=10.2, fin_top=26.2, nose="sleek", eng_x=64.5, eng_len=13.2, eng_h=5.9, eng_yc=-2.5,
        win_w=1.05, win_h=1.22, win_step=2.35, win_start=26.5, win_end=82.0,
        doors=[(85.5, False), (23.5, False)], overwing_exits=[53.5],
    )


def b_a320():
    s = Sil()
    L, h, fin_top = 100.0, 10.6, 26.8
    body = []
    body += cubic_bezier((L, h * 0.38), (L, h * 0.74), (L - 3.0, h * 0.98), (L - 8.0, h), n=16)
    body.append((25.5, h))
    body += cubic_bezier((25.5, h), (22.0, h * 1.02), (19.5, h * 1.15), (17.2, h * 1.42), n=12)
    body.append((10.0, fin_top - 0.5))
    body += quad_bezier((10.0, fin_top - 0.5), (9.4, fin_top), (8.2, fin_top), n=6)
    body.append((4.2, fin_top))
    body += quad_bezier((4.2, fin_top), (3.4, fin_top), (3.2, fin_top - 0.6), n=6)
    body.append((6.2, h * 0.86))
    body += cubic_bezier((6.2, h * 0.86), (3.5, h * 0.82), (1.2, h * 0.78), (0.0, h * 0.74), n=8)
    body.append((0.0, h * 0.62))
    body += cubic_bezier((0.0, h * 0.62), (9.5, h * 0.30), (19.5, h * 0.04), (30.0, 0.0), n=14)
    body.append((L - 8.5, 0.0))
    body += cubic_bezier((L - 8.5, 0.0), (L - 3.4, 0.0), (L, h * 0.10), (L, h * 0.38), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(5.6, fin_top), (8.2, h * 0.86)], width=0.95)
    s.stroke([(20.5, h), (8.2, h * 0.90)], width=0.8, min_w=300)
    s.stroke(cubic_bezier((L - 3.6, h * 0.77), (L - 4.1, h * 0.45), (L - 4.1, h * 0.24), (L - 3.6, h * 0.08), n=8),
             width=0.9)

    s.solid([(L - 3.3, h * 0.56), (L - 4.2, h * 0.76), (L - 6.3, h * 0.76), (L - 6.3, h * 0.56)])
    s.solid([(L - 6.8, h * 0.56), (L - 6.8, h * 0.76), (L - 8.6, h * 0.76), (L - 8.9, h * 0.58)])

    draw_door(s, 86.2, h * 0.24, h * 0.78, w=2.0)
    draw_door(s, 55.2, h * 0.36, h * 0.74, w=1.45, min_w=300)
    draw_door(s, 52.5, h * 0.36, h * 0.74, w=1.45, min_w=300)
    draw_door(s, 23.0, h * 0.27, h * 0.78, w=2.0)

    draw_window_row(s, 25.8, 83.0, h * 0.60, w=0.90, h=1.10, step=2.15, gaps=[(51.2, 56.6)])
    draw_horiz_stab(s, 15.2, -1.6, h * 0.67, thick=1.8)
    draw_wing_root(s, x_le=63.0, x_te=41.5, y_root=h * 0.14, thick=2.1,
                   fairing_le=68.5, fairing_te=37.0, belly_drop=1.35)
    for fx in (46.5, 42.5):
        s.panel(ellipse(fx, -0.8, 1.5, 0.45, n=10), fill=0, width=0.9, min_w=300)
    pylon = [(62.5, 1.5), (56.8, 2.2), (52.0, -1.4), (58.2, -1.4)]
    draw_turbofan(s, x_front=65.2, length=12.8, yc=-2.5, h=5.5, pylon_pts=pylon)
    return s


def b_a300():
    # Airbus A300/A310: stout widebody fuselage, tall wide Airbus fin, 4 doors
    return tech_airliner(
        h=11.6, fin_top=28.2, nose="airbus", fin_root_x=26.5, fin_le_tip_x=10.2, fin_te_tip_x=3.8,
        eng_x=64.5, eng_len=13.0, eng_h=6.0, eng_yc=-2.6,
        doors=[(86.5, False), (66.0, True), (44.0, True), (23.0, False)],
    )


def b_a330():
    # Airbus A330: graceful long widebody, tall swept Airbus fin, big Trent/CF6 pods
    return tech_airliner(
        h=10.3, fin_top=26.6, nose="airbus", fin_root_x=25.5, fin_le_tip_x=9.6, fin_te_tip_x=3.8,
        eng_x=65.5, eng_len=13.2, eng_h=6.1, eng_yc=-2.5,
        doors=[(87.5, False), (68.0, True), (45.0, True), (23.0, False)],
    )


def b_a340():
    # Airbus A340: long slender 4-engine widebody with staggered pairs of sleek engines
    return tech_airliner(
        h=9.4, fin_top=25.2, nose="airbus", fin_root_x=24.5, fin_le_tip_x=9.2, fin_te_tip_x=3.6,
        engines="under4", eng_x=65.5, eng_len=11.2, eng_h=4.6, eng_yc=-2.1,
        far_eng_x=50.5, far_eng_scale=0.88,
        doors=[(88.0, False), (68.5, True), (46.0, True), (22.5, False)],
    )


def b_a350():
    # Airbus A350 XWB: signature curved mask windscreen, huge Trent XWB pods, sleek tail
    return tech_airliner(
        h=10.4, fin_top=26.8, nose="a350", fin_root_x=26.0, fin_le_tip_x=9.4, fin_te_tip_x=3.8,
        eng_x=65.5, eng_len=13.8, eng_h=6.5, eng_yc=-2.6,
        doors=[(86.5, False), (67.5, True), (45.5, True), (23.0, False)],
    )


def b_a380():
    s = Sil()
    L, h, fin_top = 100.0, 12.2, 29.2

    draw_turbofan(s, x_front=50.0, length=9.6, yc=-3.0, h=4.5,
                  pylon_pts=[(47.5, 0.5), (43.5, 0.5), (42.0, -1.6), (46.0, -1.6)], far=True)

    body = []
    body += cubic_bezier((L, h * 0.34), (L, h * 0.58), (L - 2.4, h * 0.78), (L - 5.8, h * 0.90), n=14)
    body += cubic_bezier((L - 5.8, h * 0.90), (L - 8.8, h * 0.99), (L - 12.5, h), (L - 17.5, h), n=12)
    body.append((28.5, h))
    body += cubic_bezier((28.5, h), (24.5, h * 1.04), (21.2, h * 1.18), (18.2, h * 1.44), n=12)
    body.append((10.2, fin_top - 0.6))
    body += quad_bezier((10.2, fin_top - 0.6), (9.5, fin_top), (8.2, fin_top), n=6)
    body.append((3.4, fin_top))
    body += quad_bezier((3.4, fin_top), (2.6, fin_top), (2.5, fin_top - 0.6), n=6)
    body.append((6.6, h * 0.80))
    body += cubic_bezier((6.6, h * 0.80), (3.8, h * 0.76), (1.4, h * 0.72), (0.0, h * 0.68), n=8)
    body.append((0.0, h * 0.56))
    body += cubic_bezier((0.0, h * 0.56), (10.5, h * 0.28), (21.5, h * 0.04), (32.0, 0.0), n=14)
    body.append((L - 10.0, 0.0))
    body += cubic_bezier((L - 10.0, 0.0), (L - 3.8, 0.0), (L, h * 0.09), (L, h * 0.34), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(5.6, fin_top), (9.0, h * 0.80)], width=0.95)
    s.stroke([(23.0, h), (9.0, h * 0.86)], width=0.8, min_w=300)
    s.stroke(cubic_bezier((L - 3.6, h * 0.70), (L - 4.2, h * 0.44), (L - 4.2, h * 0.24), (L - 3.6, h * 0.08), n=8),
             width=0.85)

    s.solid([(L - 4.6, h * 0.51), (L - 5.7, h * 0.66), (L - 8.0, h * 0.66), (L - 8.0, h * 0.51)])
    s.solid([(L - 8.5, h * 0.51), (L - 8.5, h * 0.66), (L - 10.3, h * 0.65), (L - 10.5, h * 0.52)])

    draw_window_row(s, 29.0, 84.5, h * 0.77, w=0.82, h=0.94, step=2.1,
                    gaps=[(78.2, 81.0), (56.2, 59.0), (32.2, 35.0)])
    draw_window_row(s, 24.0, 88.5, h * 0.43, w=0.82, h=0.94, step=2.1,
                    gaps=[(84.5, 87.5), (69.2, 72.2), (53.2, 56.2), (38.2, 41.2), (24.2, 27.2)])

    for dx in (86.0, 70.7, 54.7, 39.7, 25.7):
        draw_door(s, dx, h * 0.16, h * 0.55, w=1.55, min_w=300 if dx in (70.7, 54.7, 39.7) else 0)
    for dx in (79.6, 57.6, 33.6):
        draw_door(s, dx, h * 0.63, h * 0.89, w=1.45, min_w=300)

    draw_horiz_stab(s, 16.5, -1.8, h * 0.58, thick=1.8)
    draw_wing_root(s, x_le=66.5, x_te=38.5, y_root=h * 0.12, thick=2.3,
                   fairing_le=73.0, fairing_te=33.0, belly_drop=1.45)
    draw_turbofan(s, x_front=65.0, length=10.8, yc=-2.1, h=5.2,
                  pylon_pts=[(62.5, 1.8), (57.5, 2.2), (54.5, -0.8), (59.2, -0.8)], far=False)
    return s


def b_beluga():
    # Airbus Beluga ST (A300-600ST): stouter, slightly more upright forehead step over cockpit
    s = Sil()
    L, h, bulge, fin_top = 100.0, 9.4, 21.4, 28.0

    body = []
    body += cubic_bezier((L, h * 0.36), (L, h * 0.68), (L - 2.0, h * 0.90), (L - 6.0, h * 0.96), n=12)
    body += cubic_bezier((L - 6.0, h * 0.96), (L - 9.0, h * 1.10), (L - 9.2, bulge * 0.96), (L - 19.5, bulge), n=16)
    body.append((34.5, bulge))
    body += cubic_bezier((34.5, bulge), (25.0, bulge), (23.0, h * 1.48), (18.5, h * 1.38), n=14)
    body += cubic_bezier((18.5, h * 1.38), (15.8, h * 1.38), (14.2, h * 1.78), (9.8, fin_top - 0.5), n=12)
    body += quad_bezier((9.8, fin_top - 0.5), (9.1, fin_top), (7.8, fin_top), n=6)
    body.append((3.2, fin_top))
    body += quad_bezier((3.2, fin_top), (2.4, fin_top), (2.3, fin_top - 0.5), n=6)
    body.append((6.2, h * 0.82))
    body += cubic_bezier((6.2, h * 0.82), (3.6, h * 0.78), (1.2, h * 0.74), (0.0, h * 0.70), n=8)
    body.append((0.0, h * 0.58))
    body += cubic_bezier((0.0, h * 0.58), (10.0, h * 0.28), (20.5, h * 0.04), (31.0, 0.0), n=14)
    body.append((L - 8.0, 0.0))
    body += cubic_bezier((L - 8.0, 0.0), (L - 3.2, 0.0), (L, h * 0.10), (L, h * 0.36), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(5.4, fin_top), (8.6, h * 0.82)], width=0.95)
    s.stroke(cubic_bezier((L - 6.0, h * 0.96), (L - 11.5, h * 1.00), (L - 15.0, h * 1.38), (L - 15.5, bulge), n=14),
             width=0.95)
    s.stroke([(27.0, h * 0.96), (82.0, h * 0.96)], width=0.85, min_w=300)
    s.stroke(cubic_bezier((L - 3.2, h * 0.78), (L - 3.7, h * 0.45), (L - 3.7, h * 0.24), (L - 3.2, h * 0.08), n=8),
             width=0.85)

    s.solid([(L - 3.0, h * 0.50), (L - 4.0, h * 0.70), (L - 6.2, h * 0.70), (L - 6.2, h * 0.50)])
    s.solid([(L - 6.7, h * 0.50), (L - 6.7, h * 0.70), (L - 8.3, h * 0.69), (L - 8.5, h * 0.52)])

    draw_horiz_stab(s, 14.5, -1.8, h * 0.62, thick=1.7)
    endplate = [(10.0, h * 0.56), (5.8, h * 0.56), (3.0, h * 1.28), (6.4, h * 1.28)]
    s.panel(endplate, fill=0, width=1.25)

    draw_wing_root(s, x_le=63.5, x_te=41.0, y_root=h * 0.14, thick=2.1,
                   fairing_le=68.5, fairing_te=36.5, belly_drop=1.35)
    draw_turbofan(s, x_front=64.0, length=12.4, yc=-2.5, h=5.5,
                  pylon_pts=[(61.5, 1.5), (56.5, 2.0), (52.8, -1.2), (57.8, -1.2)])
    return s


def b_belugaxl():
    s = Sil()
    L, h, bulge, fin_top = 100.0, 9.0, 21.2, 27.6

    body = []
    body += cubic_bezier((L, h * 0.36), (L, h * 0.68), (L - 2.0, h * 0.90), (L - 6.2, h * 0.95), n=12)
    body += cubic_bezier((L - 6.2, h * 0.95), (L - 7.2, h * 1.38), (L - 9.5, bulge), (L - 20.5, bulge), n=18)
    body.append((36.5, bulge))
    body += cubic_bezier((36.5, bulge), (26.5, bulge), (24.0, h * 1.50), (19.0, h * 1.42), n=14)
    body += cubic_bezier((19.0, h * 1.42), (16.2, h * 1.40), (14.6, h * 1.78), (10.0, fin_top - 0.5), n=12)
    body += quad_bezier((10.0, fin_top - 0.5), (9.3, fin_top), (8.0, fin_top), n=6)
    body.append((3.4, fin_top))
    body += quad_bezier((3.4, fin_top), (2.6, fin_top), (2.5, fin_top - 0.5), n=6)
    body.append((6.4, h * 0.82))
    body += cubic_bezier((6.4, h * 0.82), (3.6, h * 0.78), (1.2, h * 0.74), (0.0, h * 0.70), n=8)
    body.append((0.0, h * 0.58))
    body += cubic_bezier((0.0, h * 0.58), (10.0, h * 0.28), (20.5, h * 0.04), (31.0, 0.0), n=14)
    body.append((L - 8.0, 0.0))
    body += cubic_bezier((L - 8.0, 0.0), (L - 3.2, 0.0), (L, h * 0.10), (L, h * 0.36), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(5.6, fin_top), (8.8, h * 0.82)], width=0.95)
    s.stroke(cubic_bezier((L - 6.2, h * 0.95), (L - 12.0, h * 0.98), (L - 16.5, h * 1.38), (L - 17.0, bulge), n=14),
             width=0.95)
    s.stroke([(27.0, h * 0.96), (82.5, h * 0.96)], width=0.85, min_w=300)
    s.stroke(cubic_bezier((L - 3.2, h * 0.78), (L - 3.7, h * 0.45), (L - 3.7, h * 0.24), (L - 3.2, h * 0.08), n=8),
             width=0.85)

    s.solid([(L - 3.0, h * 0.50), (L - 4.0, h * 0.70), (L - 6.2, h * 0.70), (L - 6.2, h * 0.50)])
    s.solid([(L - 6.7, h * 0.50), (L - 6.7, h * 0.70), (L - 8.3, h * 0.69), (L - 8.5, h * 0.52)])
    draw_door(s, L - 13.5, h * 0.16, h * 0.66, w=1.7, min_w=300)

    draw_horiz_stab(s, 14.5, -1.8, h * 0.62, thick=1.7)
    endplate = [(10.2, h * 0.54), (5.8, h * 0.54), (2.8, h * 1.34), (6.4, h * 1.34)]
    s.panel(endplate, fill=0, width=1.25)

    draw_wing_root(s, x_le=64.0, x_te=41.5, y_root=h * 0.14, thick=2.1,
                   fairing_le=69.0, fairing_te=37.0, belly_drop=1.35)
    draw_turbofan(s, x_front=64.8, length=12.6, yc=-2.5, h=5.6,
                  pylon_pts=[(62.2, 1.5), (57.2, 2.0), (53.2, -1.2), (58.4, -1.2)])
    return s


def b_b717():
    # Boeing 717 / DC-9: pointed Douglas nose, swept T-tail, rear BR715 pod
    return tech_airliner(
        h=9.6, fin_top=24.2, nose="douglas", tail="pointed", t_tail=True,
        fin_root_x=23.5, fin_le_tip_x=7.5, fin_te_tip_x=2.4, fin_te_base_x=7.2,
        engines="rear2", eng_x=31.0, eng_len=13.0, eng_h=4.6,
        wing_le=58.0, wing_te=37.0, belly_drop=0.9,
        win_start=34.0, win_end=82.0, doors=[(85.5, False)], overwing_exits=[51.0],
    )


def b_b727():
    # Boeing 727: classic trijet — swept T-tail, dorsal S-duct #2 intake + rear side pods!
    return tech_airliner(
        h=9.4, fin_top=24.8, nose="boeing", tail="pointed", t_tail=True, s_duct=True,
        fin_root_x=23.0, fin_le_tip_x=6.5, fin_te_tip_x=1.8, fin_te_base_x=7.0,
        engines="rear3", eng_x=30.5, eng_len=14.0, eng_h=4.0, eng_slim=True,
        wing_le=58.5, wing_te=36.5, belly_drop=0.9,
        win_start=33.5, win_end=84.0, doors=[(87.0, False)], overwing_exits=[52.0, 49.4],
    )


def b_b737():
    # Boeing 737 NG: signature pointed Boeing nose + iconic 737 dorsal fin kink!
    return tech_airliner(
        h=9.9, fin_top=25.6, nose="boeing", dorsal_kink=True,
        fin_root_x=24.5, fin_le_tip_x=9.4, fin_te_tip_x=3.8, fin_te_base_x=6.2,
        eng_x=64.5, eng_len=11.5, eng_h=5.0, eng_yc=-2.0,
        doors=[(85.5, False), (23.5, False)], overwing_exits=[54.5, 51.8],
    )


def b_b737max():
    # Boeing 737 MAX: dorsal fin kink + larger forward LEAP-1B pod with chevron cowl!
    return tech_airliner(
        h=9.9, fin_top=25.6, nose="boeing", tail="pointed", dorsal_kink=True,
        fin_root_x=24.5, fin_le_tip_x=9.4, fin_te_tip_x=3.8, fin_te_base_x=6.2,
        eng_x=66.5, eng_len=13.2, eng_h=5.8, eng_yc=-2.2, eng_chevron=True,
        doors=[(85.5, False), (23.5, False)], overwing_exits=[54.5, 51.8],
    )


def b_b747():
    s = Sil()
    L, h, hh, fin_top = 100.0, 9.4, 14.2, 25.0

    draw_turbofan(s, x_front=49.5, length=9.2, yc=-3.2, h=4.2,
                  pylon_pts=[(47.2, 0.5), (43.5, 0.5), (42.0, -1.8), (45.8, -1.8)], far=True)

    body = []
    body += cubic_bezier((L, h * 0.35), (L, h * 0.62), (L - 3.2, h * 0.88), (L - 7.0, h * 1.10), n=14)
    body += cubic_bezier((L - 7.0, h * 1.10), (L - 10.2, hh * 0.96), (L - 14.5, hh), (L - 19.5, hh), n=14)
    body.append((66.0, hh))
    body += cubic_bezier((66.0, hh), (58.0, hh), (56.0, h), (47.5, h), n=18)
    body.append((26.0, h))
    body += cubic_bezier((26.0, h), (22.5, h * 1.05), (19.5, h * 1.25), (16.5, h * 1.55), n=12)
    body.append((8.2, fin_top - 0.5))
    body += quad_bezier((8.2, fin_top - 0.5), (7.6, fin_top), (6.5, fin_top), n=6)
    body.append((2.4, fin_top))
    body += quad_bezier((2.4, fin_top), (1.8, fin_top), (1.7, fin_top - 0.5), n=6)
    body.append((6.0, h * 0.84))
    body += cubic_bezier((6.0, h * 0.84), (3.4, h * 0.78), (1.2, h * 0.74), (0.0, h * 0.70), n=8)
    body.append((0.0, h * 0.58))
    body += cubic_bezier((0.0, h * 0.58), (9.5, h * 0.28), (19.5, h * 0.04), (30.0, 0.0), n=14)
    body.append((L - 9.5, 0.0))
    body += cubic_bezier((L - 9.5, 0.0), (L - 3.8, 0.0), (L, h * 0.10), (L, h * 0.35), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(4.2, fin_top), (7.8, h * 0.84)], width=0.95)
    s.stroke([(21.0, h), (7.8, h * 0.88)], width=0.8, min_w=300)
    s.stroke(cubic_bezier((L - 3.8, h * 0.74), (L - 4.4, h * 0.44), (L - 4.4, h * 0.24), (L - 3.8, h * 0.08), n=8),
             width=0.85)

    s.solid([(L - 8.6, hh * 0.74), (L - 10.2, hh * 0.86), (L - 12.5, hh * 0.86), (L - 12.3, hh * 0.74)])
    s.solid([(L - 12.9, hh * 0.74), (L - 13.1, hh * 0.86), (L - 14.6, hh * 0.85), (L - 14.5, hh * 0.74)])

    draw_window_row(s, 64.5, 82.8, hh * 0.79, w=0.82, h=0.96, step=2.05, gaps=[(75.2, 78.2)])
    draw_door(s, 76.7, hh * 0.63, hh * 0.89, w=1.45, min_w=300)

    for dx in (88.0, 72.5, 55.0, 39.0, 22.5):
        draw_door(s, dx, h * 0.21, h * 0.74, w=1.65, min_w=300 if dx in (55.0, 39.0) else 0)
    draw_window_row(s, 25.0, 93.5, h * 0.55, w=0.82, h=0.96, step=2.0,
                    gaps=[(86.5, 89.5), (71.0, 74.0), (53.5, 56.5), (37.5, 40.5)])

    draw_horiz_stab(s, 14.8, -1.8, h * 0.62, thick=1.7)
    draw_wing_root(s, x_le=65.5, x_te=41.5, y_root=h * 0.14, thick=2.0,
                   fairing_le=70.5, fairing_te=36.5, belly_drop=1.2)
    draw_turbofan(s, x_front=65.2, length=10.4, yc=-2.1, h=4.7,
                  pylon_pts=[(62.8, 1.5), (58.2, 1.8), (55.2, -1.0), (59.8, -1.0)], far=False)
    return s


def b_dreamlifter():
    # Boeing 747-400 LCF Dreamlifter: 747 nose & cockpit + massive swollen cargo hold + swing-tail seam!
    s = Sil()
    L, h, hh, bulge, fin_top = 100.0, 9.4, 14.0, 19.8, 25.8

    draw_turbofan(s, x_front=49.5, length=9.2, yc=-3.2, h=4.2,
                  pylon_pts=[(47.2, 0.5), (43.5, 0.5), (42.0, -1.8), (45.8, -1.8)], far=True)

    body = []
    # 747 nose + upper-deck flight deck brow
    body += cubic_bezier((L, h * 0.35), (L, h * 0.62), (L - 3.2, h * 0.88), (L - 7.0, h * 1.10), n=14)
    body += cubic_bezier((L - 7.0, h * 1.10), (L - 10.0, hh * 0.96), (L - 14.0, hh), (L - 18.0, hh), n=12)
    # Huge stepped Dreamlifter cargo bulge rising right behind the flight deck!
    body += cubic_bezier((L - 18.0, hh), (L - 22.0, hh * 1.04), (L - 23.0, bulge), (L - 32.0, bulge), n=14)
    body.append((34.0, bulge))
    # Rear swing-tail taper down into the tall swept 747 vertical fin
    body += cubic_bezier((34.0, bulge), (25.5, bulge), (23.5, h * 1.38), (19.0, h * 1.28), n=14)
    body += cubic_bezier((19.0, h * 1.28), (16.0, h * 1.32), (13.2, h * 1.78), (8.4, fin_top - 0.5), n=12)
    body += quad_bezier((8.4, fin_top - 0.5), (7.8, fin_top), (6.6, fin_top), n=6)
    body.append((2.4, fin_top))
    body += quad_bezier((2.4, fin_top), (1.8, fin_top), (1.7, fin_top - 0.5), n=6)
    body.append((6.0, h * 0.84))
    body += cubic_bezier((6.0, h * 0.84), (3.4, h * 0.78), (1.2, h * 0.74), (0.0, h * 0.70), n=8)
    body.append((0.0, h * 0.58))
    body += cubic_bezier((0.0, h * 0.58), (9.5, h * 0.28), (19.5, h * 0.04), (30.0, 0.0), n=14)
    body.append((L - 9.5, 0.0))
    body += cubic_bezier((L - 9.5, 0.0), (L - 3.8, 0.0), (L, h * 0.10), (L, h * 0.35), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(4.2, fin_top), (7.8, h * 0.84)], width=0.95)
    # Iconic Dreamlifter swing-tail fuselage split seam!
    s.stroke([(27.5, bulge * 0.94), (27.5, h * 0.08)], width=1.0)
    # Lower fuselage datum crease & radome seam
    s.stroke([(28.5, h * 0.98), (74.0, h * 0.98)], width=0.85, min_w=300)
    s.stroke(cubic_bezier((L - 3.8, h * 0.74), (L - 4.4, h * 0.44), (L - 4.4, h * 0.24), (L - 3.8, h * 0.08), n=8),
             width=0.85)

    # Upper-deck cockpit glazing + crew door
    s.solid([(L - 8.6, hh * 0.74), (L - 10.2, hh * 0.86), (L - 12.5, hh * 0.86), (L - 12.3, hh * 0.74)])
    s.solid([(L - 12.9, hh * 0.74), (L - 13.1, hh * 0.86), (L - 14.6, hh * 0.85), (L - 14.5, hh * 0.74)])
    draw_door(s, 86.0, h * 0.21, h * 0.74, w=1.65)

    draw_horiz_stab(s, 14.8, -1.8, h * 0.62, thick=1.7)
    draw_wing_root(s, x_le=65.5, x_te=41.5, y_root=h * 0.14, thick=2.0,
                   fairing_le=70.5, fairing_te=36.5, belly_drop=1.2)
    draw_turbofan(s, x_front=65.2, length=10.4, yc=-2.1, h=4.7,
                  pylon_pts=[(62.8, 1.5), (58.2, 1.8), (55.2, -1.0), (59.8, -1.0)], far=False)
    return s


def b_b757():
    # Boeing 757 ("Flying Pencil"): long slender body, droop-brow nose, forward RB211/PW2000 pods
    return tech_airliner(
        h=8.6, fin_top=23.6, nose="boeing",
        fin_root_x=24.0, fin_le_tip_x=9.0, fin_te_tip_x=3.5, fin_te_base_x=6.0,
        eng_x=66.5, eng_len=12.6, eng_h=5.4, eng_yc=-2.3,
        doors=[(87.5, False), (70.0, True), (45.0, True), (22.5, False)],
    )


def b_b767():
    # Boeing 767: medium widebody, pointed Boeing nose, 3 doors + overwing exits
    return tech_airliner(
        h=10.6, fin_top=26.6, nose="boeing",
        fin_root_x=25.5, fin_le_tip_x=9.5, fin_te_tip_x=3.8, fin_te_base_x=6.2,
        eng_x=65.0, eng_len=13.0, eng_h=5.9, eng_yc=-2.5,
        doors=[(86.5, False), (66.5, True), (23.0, False)], overwing_exits=[52.0, 49.4],
    )


def b_b777():
    # Boeing 777: chisel blade APU tailcone, massive GE90 turbofan, 5 doors!
    return tech_airliner(
        h=10.4, fin_top=26.2, nose="airbus", tail="chisel",
        fin_root_x=25.5, fin_le_tip_x=9.2, fin_te_tip_x=3.6, fin_te_base_x=6.4,
        eng_x=66.0, eng_len=14.6, eng_h=7.0, eng_yc=-2.8,
        doors=[(88.0, False), (71.5, True), (55.0, True), (39.0, True), (22.5, False)],
    )


def b_b787():
    # Boeing 787 Dreamliner: continuous smooth curved nose, larger windows, chevron turbofan!
    return tech_airliner(
        h=10.4, fin_top=26.4, nose="sleek", tail="pointed",
        fin_root_x=26.0, fin_le_tip_x=8.8, fin_te_tip_x=3.4, fin_te_base_x=6.5,
        eng_x=65.5, eng_len=13.6, eng_h=6.4, eng_yc=-2.6, eng_chevron=True,
        win_w=1.02, win_h=1.22, win_step=2.3,
        doors=[(86.0, False), (67.0, True), (45.0, True), (23.0, False)],
    )


def b_md80():
    # MD-80 / MD-90 ("Mad Dog"): very long slender fuselage, pointed Douglas nose, long slim rear pod
    return tech_airliner(
        h=8.0, fin_top=21.8, nose="douglas", tail="pointed", t_tail=True,
        fin_root_x=21.5, fin_le_tip_x=6.2, fin_te_tip_x=1.8, fin_te_base_x=6.5,
        engines="rear2", eng_x=30.0, eng_len=14.5, eng_h=3.8, eng_slim=True,
        wing_le=56.0, wing_te=35.5, belly_drop=0.8, tail_upsweep_x=26.0,
        win_start=32.5, win_end=84.5, win_h=0.95,
        doors=[(87.5, False)], overwing_exits=[51.0, 48.5],
    )


def b_dc10():
    # DC-10 / MD-11: iconic straight-through #2 tail engine embedded in fin base!
    return tech_airliner(
        h=10.8, fin_top=27.2, nose="airbus",
        fin_root_x=24.0, fin_le_tip_x=9.0, fin_te_tip_x=3.4, fin_te_base_x=6.5,
        engines="tail3", eng_x=64.5, eng_len=12.6, eng_h=5.8, eng_yc=-2.5,
        doors=[(87.0, False), (68.0, True), (46.0, True), (26.0, False)],
    )


def b_ejet():
    # Embraer E-Jet (E175/E190/E195): sleek pointed nose, compact underwing pods
    return tech_airliner(
        h=10.4, fin_top=26.4, nose="boeing",
        fin_root_x=25.5, fin_le_tip_x=9.5, fin_te_tip_x=3.8, fin_te_base_x=6.2,
        eng_x=64.0, eng_len=11.8, eng_h=5.3, eng_yc=-2.1,
        doors=[(84.5, False), (24.0, False)],
    )


def b_erj():
    # Embraer ERJ-135/145: ultra-slender pointed regional jet with swept T-tail
    return tech_airliner(
        h=7.8, fin_top=21.2, nose="douglas", tail="pointed", t_tail=True,
        fin_root_x=24.0, fin_le_tip_x=7.2, fin_te_tip_x=2.4, fin_te_base_x=6.8,
        engines="rear2", eng_x=31.0, eng_len=12.5, eng_h=4.0, eng_slim=True,
        wing_le=58.0, wing_te=38.0, belly_drop=0.8, tail_upsweep_x=28.0,
        win_start=34.0, win_end=82.0, win_h=0.92,
        doors=[(85.5, False)], overwing_exits=[51.0],
    )


def b_crj():
    # Bombardier CRJ-700/900: slender pointed nose, taller T-tail, rear CF34 pod
    return tech_airliner(
        h=8.4, fin_top=23.0, nose="douglas", tail="pointed", t_tail=True,
        fin_root_x=23.5, fin_le_tip_x=7.0, fin_te_tip_x=2.2, fin_te_base_x=6.6,
        engines="rear2", eng_x=31.5, eng_len=13.0, eng_h=4.5,
        wing_le=57.5, wing_te=37.5, belly_drop=0.9, tail_upsweep_x=28.0,
        win_start=34.5, win_end=82.5, win_h=0.98,
        doors=[(85.5, False)], overwing_exits=[50.5],
    )


def b_atr():
    # ATR 42 / 72: high wing, T-tail, bulky gear sponson, PW127 turboprop + 6-blade prop disc
    return tech_high_winger(
        h=10.6, fin_top=26.6, nose="round", t_tail=True, upsweep=False,
        fin_root_x=24.5, fin_le_tip_x=9.5, fin_te_tip_x=3.6, fin_te_base_x=6.2,
        engines="tp2", eng_x=65.5, eng_len=15.0, eng_h=4.5, prop_r=7.6,
        sponson=True, sponson_x=(42.0, 66.0), wins=True,
        doors=[(83.5, False), (25.5, False)],
    )


def b_dash8():
    # De Havilland Dash 8 / Q400: sleeker pointed nose, extra-long slender turboprop nacelle, tall T-tail
    return tech_high_winger(
        h=9.6, fin_top=26.8, nose="pointed", t_tail=True, upsweep=False,
        fin_root_x=24.0, fin_le_tip_x=9.2, fin_te_tip_x=3.4, fin_te_base_x=6.2,
        engines="tp2", eng_x=68.0, eng_len=24.5, eng_h=4.2, prop_r=8.0,
        sponson=False, wins=True,
        doors=[(83.0, False), (25.5, False)],
    )


def b_saab():
    # Saab 340 / 2000: low-wing regional turboprop with conventional tail!
    def add_prop(s, h):
        draw_turboprop(s, x_front=67.5, length=18.5, yc=0.2, h=4.4, prop_r=7.8, far=False)

    return tech_airliner(
        h=10.2, fin_top=25.4, nose="boeing", tail="pointed",
        fin_root_x=25.0, fin_le_tip_x=10.5, fin_te_tip_x=4.5, fin_te_base_x=6.5,
        engines="none", wing_le=63.0, wing_te=42.5, belly_drop=1.0,
        win_start=29.0, win_end=80.5, win_step=2.4,
        doors=[(83.5, False)], extra_fn=add_prop,
    )


def b_f100():
    # Fokker 70 / 100: rounded nose, T-tail, rear Tay pod, split tailcone airbrake!
    return tech_airliner(
        h=9.2, fin_top=23.6, nose="airbus", tail="pointed", t_tail=True, split_airbrake=True,
        fin_root_x=23.5, fin_le_tip_x=7.8, fin_te_tip_x=2.6, fin_te_base_x=6.8,
        engines="rear2", eng_x=31.5, eng_len=12.8, eng_h=4.6,
        wing_le=58.0, wing_te=38.0, belly_drop=0.9,
        win_start=34.5, win_end=83.0, doors=[(86.0, False)], overwing_exits=[51.0],
    )


def b_bae146():
    # BAe 146 / Avro RJ: plump high-wing 4-engine regional jet with T-tail & tailcone airbrake!
    return tech_high_winger(
        h=11.0, fin_top=27.2, nose="blunt", t_tail=True, upsweep=False, airbrake=True,
        fin_root_x=25.5, fin_le_tip_x=9.2, fin_te_tip_x=3.4, fin_te_base_x=6.6,
        engines="jet4", eng_x=65.5, eng_len=10.8, eng_h=4.5,
        sponson=True, sponson_x=(42.0, 66.0), wins=True,
        doors=[(85.0, False), (25.5, False)],
    )


def b_comac():
    # COMAC C919: modern narrowbody with sleek curved nose and LEAP-1C turbofan
    return tech_airliner(
        h=10.5, fin_top=26.6, nose="sleek",
        fin_root_x=25.5, fin_le_tip_x=9.8, fin_te_tip_x=4.0, fin_te_base_x=6.2,
        eng_x=65.0, eng_len=13.0, eng_h=5.7, eng_yc=-2.4,
        doors=[(85.5, False), (23.5, False)], overwing_exits=[54.0, 51.5],
    )


def b_superjet():
    # Sukhoi Superjet 100: compact regional twinjet with pointed nose & tall fin
    return tech_airliner(
        h=10.4, fin_top=26.8, nose="boeing",
        fin_root_x=25.0, fin_le_tip_x=9.4, fin_te_tip_x=3.8, fin_te_base_x=6.2,
        eng_x=64.0, eng_len=12.0, eng_h=5.4, eng_yc=-2.1,
        doors=[(84.5, False), (24.0, False)],
    )


# ---- Heavy Cargo Transports (Civil & Military) -----------------------------


def b_il76():
    # Ilyushin Il-76: high wing, T-tail, glazed navigator nose, 4 slender turbofans
    return tech_high_winger(
        h=11.4, fin_top=27.6, nose="glazed", t_tail=True, upsweep=True,
        fin_root_x=25.5, fin_le_tip_x=8.8, fin_te_tip_x=3.0, fin_te_base_x=6.8,
        engines="jet4", eng_x=66.5, eng_len=13.2, eng_h=4.5,
        sponson=True, sponson_x=(41.0, 65.0), portholes=True,
    )


def b_an124():
    # Antonov An-124 Ruslan: huge high-wing quadjet, visor nose, LOW conventional tail!
    return tech_high_winger(
        h=13.0, fin_top=29.5, nose="visor", t_tail=False, upsweep=True,
        fin_root_x=27.5, fin_le_tip_x=10.2, fin_te_tip_x=3.8, fin_te_base_x=6.8,
        engines="jet4", eng_x=66.0, eng_len=13.2, eng_h=5.8,
        sponson=True, sponson_x=(39.0, 68.0), portholes=True,
    )


def b_an225():
    # Antonov An-225 Mriya: 6 underwing turbofans (3 per side) + iconic twin H-tail!
    return tech_high_winger(
        h=11.8, fin_top=24.8, nose="visor", t_tail=False, h_tail=True, upsweep=True,
        engines="jet6", eng_x=68.5, eng_len=11.8, eng_h=5.1,
        sponson=True, sponson_x=(38.0, 68.0), portholes=True,
    )


def b_superguppy():
    # Aero Spacelines Super Guppy: enormous ballooning hold over low nose + 4 turboprops!
    s = Sil()
    L, h, bulge, fin_top = 100.0, 9.2, 24.4, 27.2

    # Far outboard turboprop
    draw_turboprop(s, x_front=55.5, length=12.5, yc=-0.4, h=3.8, prop_r=6.8, far=True)

    body = []
    # Low Stratocruiser nose & cockpit
    body += cubic_bezier((L, h * 0.36), (L, h * 0.68), (L - 2.0, h * 0.88), (L - 5.5, h * 0.94), n=12)
    # Huge bulging spherical-front Super Guppy hold!
    body += cubic_bezier((L - 5.5, h * 0.94), (L - 3.0, bulge * 0.88), (L - 12.0, bulge), (L - 26.0, bulge), n=18)
    body.append((34.0, bulge))
    # Rear taper into tall classic Boeing 377 vertical tail
    body += cubic_bezier((34.0, bulge), (24.0, bulge), (21.5, h * 1.42), (17.5, h * 1.32), n=14)
    body.append((13.0, fin_top - 0.8))
    body += quad_bezier((13.0, fin_top - 0.8), (12.0, fin_top), (10.5, fin_top), n=6)
    body.append((5.2, fin_top))
    body += quad_bezier((5.2, fin_top), (4.2, fin_top), (4.0, fin_top - 0.8), n=6)
    body.append((5.5, h * 0.82))
    body += cubic_bezier((5.5, h * 0.82), (3.2, h * 0.78), (1.0, h * 0.74), (0.0, h * 0.70), n=8)
    body.append((0.0, h * 0.56))
    body += cubic_bezier((0.0, h * 0.56), (10.0, h * 0.28), (20.5, h * 0.04), (30.0, 0.0), n=14)
    body.append((L - 8.0, 0.0))
    body += cubic_bezier((L - 8.0, 0.0), (L - 3.0, 0.0), (L, h * 0.10), (L, h * 0.36), n=14)
    s.panel(body, fill=0, width=1.6)

    # Rudder hinge line + iconic Super Guppy hinged-nose swing seam!
    s.stroke([(7.2, fin_top), (8.2, h * 0.82)], width=0.95)
    s.stroke([(71.5, bulge), (71.5, 0.0)], width=1.0)
    s.stroke([(27.0, h * 0.96), (71.5, h * 0.96)], width=0.85, min_w=300)

    # Low cockpit glazing
    s.solid([(L - 2.8, h * 0.48), (L - 3.8, h * 0.68), (L - 6.0, h * 0.68), (L - 6.0, h * 0.48)])
    s.solid([(L - 6.5, h * 0.48), (L - 6.5, h * 0.68), (L - 8.2, h * 0.67), (L - 8.4, h * 0.50)])

    draw_horiz_stab(s, 14.5, -1.5, h * 0.64, thick=1.7)
    draw_wing_root(s, x_le=65.0, x_te=42.5, y_root=h * 0.14, thick=2.0,
                   fairing_le=69.0, fairing_te=38.0, belly_drop=1.0)
    # Near inboard turboprop
    draw_turboprop(s, x_front=66.5, length=14.0, yc=-1.2, h=4.2, prop_r=7.6, far=False)
    return s


def b_concorde():
    s = Sil()
    L, h, fin_top = 100.0, 5.6, 18.2

    body = []
    body += cubic_bezier((L, h * 0.18), (L - 5.5, h * 0.42), (L - 12.5, h * 0.82), (L - 19.0, h), n=18)
    body.append((32.0, h))
    body += cubic_bezier((32.0, h), (26.0, h * 1.08), (21.5, h * 1.50), (12.2, fin_top - 0.3), n=16)
    body += quad_bezier((12.2, fin_top - 0.3), (11.6, fin_top), (10.5, fin_top), n=5)
    body.append((7.2, fin_top))
    body.append((10.5, h * 0.86))
    body += cubic_bezier((10.5, h * 0.86), (6.0, h * 0.66), (2.2, h * 0.50), (0.0, h * 0.42), n=10)
    body.append((0.0, h * 0.30))
    body += cubic_bezier((0.0, h * 0.30), (6.5, h * 0.15), (14.0, 0.02), (22.0, 0.0), n=12)
    body.append((L - 17.0, 0.0))
    body += cubic_bezier((L - 17.0, 0.0), (L - 9.0, 0.0), (L - 3.5, h * 0.08), (L, h * 0.18), n=14)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(9.0, fin_top), (12.2, h * 0.86)], width=0.95)
    s.stroke([(10.6, h * 2.1), (8.8, h * 2.1)], width=0.8, min_w=300)
    s.stroke([(L - 11.8, h * 0.72), (L - 12.2, 0.0)], width=0.9)
    s.solid([(L - 11.6, h * 0.56), (L - 13.8, h * 0.80), (L - 16.5, h * 0.82), (L - 16.5, h * 0.62)])
    s.solid([(L - 17.0, h * 0.62), (L - 17.0, h * 0.82), (L - 18.6, h * 0.80), (L - 18.6, h * 0.63)])

    draw_window_row(s, 27.0, 77.0, h * 0.62, w=0.72, h=0.82, step=1.85,
                    gaps=[(73.0, 75.8), (49.2, 52.0)])
    draw_door(s, 74.4, h * 0.16, h * 0.78, w=1.45, min_w=300)
    draw_door(s, 50.6, h * 0.16, h * 0.78, w=1.45, min_w=300)

    wing = [(68.5, h * 0.22), (56.0, h * 0.38), (16.5, h * 0.18), (13.5, -0.35), (45.0, -0.55), (68.5, h * 0.08)]
    s.panel(wing, fill=0, width=1.35)

    nacelle = [(41.5, -0.35), (19.5, -0.35), (18.5, -3.1), (40.0, -3.1)]
    s.panel(nacelle, fill=0, width=1.35)
    s.stroke([(41.5, -0.35), (38.8, -3.1)], width=1.0)
    s.solid([(41.0, -0.55), (39.6, -0.55), (38.6, -2.9), (39.7, -2.9)])
    s.panel([(19.5, -0.55), (15.6, -0.85), (15.6, -2.8), (18.8, -3.0)], fill=0, width=1.15)
    s.solid([(16.8, -1.05), (15.5, -1.18), (15.5, -2.55), (16.8, -2.65)])
    return s


# ---- Business Jets & General Aviation --------------------------------------


def b_lightjet():
    # Light Business Jet (Citation / Phenom): compact body, cruciform tail, oval executive windows
    return tech_airliner(
        h=11.6, fin_top=28.5, nose="douglas", tail="pointed",
        t_tail=False, cruciform_y=15.4,
        fin_root_x=27.0, fin_le_tip_x=10.5, fin_te_tip_x=3.8, fin_te_base_x=7.2,
        engines="rear2", eng_x=36.5, eng_len=15.0, eng_h=5.8,
        wing_le=60.0, wing_te=39.0, belly_drop=1.0, tail_upsweep_x=36.0,
        win_start=43.0, win_end=76.5, win_w=2.1, win_h=2.4, win_step=5.8, win_oval=True,
        doors=[(80.5, False)],
    )


def b_bizjet():
    # Heavy Long-Range Business Jet (Gulfstream G650 / Global 7500): swept T-tail, panoramic oval windows!
    return tech_airliner(
        h=10.4, fin_top=26.6, nose="douglas", tail="pointed", t_tail=True,
        fin_root_x=26.5, fin_le_tip_x=8.5, fin_te_tip_x=2.6, fin_te_base_x=7.2,
        engines="rear2", eng_x=35.5, eng_len=15.0, eng_h=5.4,
        wing_le=60.5, wing_te=38.5, belly_drop=1.0, tail_upsweep_x=34.0,
        win_start=41.5, win_end=77.0, win_w=2.5, win_h=2.2, win_step=5.6, win_oval=True,
        doors=[(81.5, False)],
    )


def _tech_light_ga(high=True):
    """Technical side profiles for single-engine GA (Cessna 172 vs Cirrus SR22/Piper)."""
    s = Sil()
    L = 100.0
    h = 15.8 if high else 14.8
    fin_top = 31.8 if high else 29.8

    # Unified fuselage + tail fin
    body = []
    if high:
        # Cessna 172: boxy cowling, steep windscreen, high cabin roof, slim tailcone, classic swept fin!
        body += cubic_bezier((95.5, h * 0.56), (95.5, h * 0.74), (92.0, h * 0.78), (79.5, h * 0.80), n=10)
        # Steep Cessna windscreen up to high wing cabin roof
        body.append((72.5, h * 1.12))
        body.append((54.5, h * 1.10))
        # Rear cabin window drop down to slender tailcone spine
        body += cubic_bezier((54.5, h * 1.10), (49.0, h * 1.08), (46.0, h * 0.78), (38.0, h * 0.74), n=10)
        body.append((25.5, h * 0.72))
        # Signature Cessna swept dorsal fillet & tail fin
        body += cubic_bezier((25.5, h * 0.72), (20.0, h * 0.82), (16.5, h * 1.12), (10.2, fin_top - 0.5), n=12)
        body += quad_bezier((10.2, fin_top - 0.5), (9.4, fin_top), (8.2, fin_top), n=5)
        body.append((3.4, fin_top))
        body += quad_bezier((3.4, fin_top), (2.6, fin_top), (2.5, fin_top - 0.6), n=5)
        body.append((4.2, h * 0.50))
        body.append((2.0, h * 0.48))
        body.append((2.0, h * 0.36))
        body += cubic_bezier((2.0, h * 0.36), (22.0, h * 0.22), (42.0, 0.0), (54.0, 0.0), n=12)
        body.append((82.0, 0.0))
        body += cubic_bezier((82.0, 0.0), (92.0, h * 0.04), (95.5, h * 0.16), (95.5, h * 0.56), n=12)
    else:
        # Cirrus SR22 / Piper Cherokee: sleek streamlined cowling, smooth domed cabin canopy!
        body += cubic_bezier((95.5, h * 0.46), (95.5, h * 0.68), (91.0, h * 0.74), (80.0, h * 0.76), n=10)
        # Smooth aerodynamic raked windscreen & teardrop cabin roof
        body += cubic_bezier((80.0, h * 0.76), (73.5, h * 1.18), (58.0, h * 1.24), (44.0, h * 0.88), n=16)
        body.append((24.0, h * 0.74))
        body += cubic_bezier((24.0, h * 0.74), (19.5, h * 0.82), (16.0, h * 1.08), (10.5, fin_top - 0.5), n=12)
        body += quad_bezier((10.5, fin_top - 0.5), (9.6, fin_top), (8.4, fin_top), n=5)
        body.append((3.6, fin_top))
        body += quad_bezier((3.6, fin_top), (2.8, fin_top), (2.7, fin_top - 0.6), n=5)
        body.append((4.4, h * 0.52))
        body.append((1.8, h * 0.48))
        body.append((1.8, h * 0.36))
        body += cubic_bezier((1.8, h * 0.36), (22.0, h * 0.20), (42.0, 0.0), (54.0, 0.0), n=12)
        body.append((82.5, 0.0))
        body += cubic_bezier((82.5, 0.0), (92.0, h * 0.04), (95.5, h * 0.14), (95.5, h * 0.46), n=12)

    s.panel(body, fill=0, width=1.6)
    # Rudder hinge + engine cowling firewall seam
    s.stroke([(5.8, fin_top), (6.8, h * 0.52)], width=0.95)
    s.stroke([(80.5, h * 0.76), (80.5, h * 0.04)], width=0.9)

    # Cabin windows
    if high:
        # Cessna 172 front windscreen + main door window + Omni-Vision rear wrap window!
        s.solid([(77.8, h * 0.76), (72.6, h * 1.02), (61.5, h * 1.02), (61.5, h * 0.62), (76.2, h * 0.62)])
        s.solid([(59.8, h * 0.62), (59.8, h * 1.00), (53.2, h * 0.98), (47.5, h * 0.68), (47.5, h * 0.62)])
        draw_door(s, 68.0, h * 0.14, h * 1.02, w=12.5, min_w=300)
    else:
        # Cirrus / Piper sleek 3-pane side canopy windows
        s.solid([(77.5, h * 0.68), (72.0, h * 1.02), (64.2, h * 1.04), (64.2, h * 0.62), (76.5, h * 0.62)])
        s.solid([(62.4, h * 0.62), (62.4, h * 1.04), (55.2, h * 0.98), (55.2, h * 0.62)])
        s.solid([(53.6, h * 0.62), (53.6, h * 0.94), (48.2, h * 0.76), (48.2, h * 0.62)])

    # Horizontal stabilizer
    draw_horiz_stab(s, 14.5, -1.5, h * 0.46, thick=2.0)

    # Fixed tricycle landing gear (nose + main wheels with wheel pants!)
    draw_fixed_wheel(s, 84.5, -5.8, 0.4, r=2.6, pant=True, strut_dx=-1.5)
    draw_fixed_wheel(s, 58.0, -6.0, 0.4, r=2.9, pant=True, strut_dx=2.0)

    # Wing (+ diagonal strut on high-wing Cessna)
    if high:
        draw_high_wing_box(s, x_le=73.5, x_te=52.5, y_top=h * 1.12, thick=2.4)
        s.stroke([(64.5, h * 1.08), (57.5, 0.5)], width=1.25)
    else:
        draw_wing_root(s, x_le=74.0, x_te=50.5, y_root=h * 0.14, thick=2.5)

    # Nose propeller disc & spinner
    px = 97.0
    yc = h * (0.52 if high else 0.46)
    pr = 12.2
    s.panel([(px - 0.5, yc - pr), (px + 0.5, yc - pr), (px + 0.8, yc),
             (px + 0.5, yc + pr), (px - 0.5, yc + pr), (px - 0.8, yc)], fill=1, width=0.85)
    s.panel([(95.2, yc + 2.4), (97.8, yc + 1.8), (100.5, yc), (97.8, yc - 1.8), (95.2, yc - 2.4)],
            fill=0, width=1.2)
    return s


def b_single_high():
    return _tech_light_ga(high=True)


def b_single_low():
    return _tech_light_ga(high=False)


def b_twin_piston():
    # Beechcraft Baron / Piper Seneca: sleek pointed nose, 4 cabin windows, low wing + piston nacelle
    s = Sil()
    L, h, fin_top = 100.0, 13.6, 29.5

    body = []
    body += cubic_bezier((L, h * 0.34), (L - 1.0, h * 0.56), (L - 6.0, h * 0.68), (L - 14.0, h * 0.74), n=12)
    # Raked windscreen up to cabin crown
    body += cubic_bezier((L - 14.0, h * 0.74), (L - 18.0, h * 1.02), (L - 22.0, h * 1.05), (L - 28.0, h * 1.05), n=10)
    body.append((44.0, h * 1.00))
    body += cubic_bezier((44.0, h * 1.00), (34.0, h * 0.90), (28.0, h * 0.76), (23.5, h * 0.74), n=10)
    body += cubic_bezier((23.5, h * 0.74), (19.0, h * 0.84), (15.5, h * 1.12), (9.8, fin_top - 0.5), n=12)
    body += quad_bezier((9.8, fin_top - 0.5), (9.0, fin_top), (7.8, fin_top), n=5)
    body.append((3.2, fin_top))
    body += quad_bezier((3.2, fin_top), (2.5, fin_top), (2.4, fin_top - 0.5), n=5)
    body.append((5.2, h * 0.62))
    body += cubic_bezier((5.2, h * 0.62), (2.8, h * 0.58), (1.0, h * 0.54), (0.0, h * 0.50), n=6)
    body.append((0.0, h * 0.38))
    body += cubic_bezier((0.0, h * 0.38), (16.0, h * 0.22), (32.0, 0.0), (44.0, 0.0), n=12)
    body.append((L - 14.0, 0.0))
    body += cubic_bezier((L - 14.0, 0.0), (L - 5.0, h * 0.04), (L, h * 0.14), (L, h * 0.34), n=12)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(5.4, fin_top), (7.4, h * 0.62)], width=0.95)
    s.stroke([(L - 13.0, h * 0.72), (L - 13.0, h * 0.04)], width=0.85)
    # Windscreen + 4 cabin windows
    s.solid([(L - 14.2, h * 0.62), (L - 18.2, h * 0.94), (L - 24.5, h * 0.94), (L - 24.5, h * 0.60)])
    draw_window_row(s, 46.5, 71.0, h * 0.74, w=3.6, h=2.6, step=6.4)

    draw_horiz_stab(s, 14.5, -1.5, h * 0.50, thick=2.0)
    draw_wing_root(s, x_le=71.0, x_te=47.0, y_root=h * 0.12, thick=2.2)
    # Wing-mounted piston engine nacelle + prop
    draw_turboprop(s, x_front=78.5, length=21.0, yc=h * 0.10, h=5.0, prop_r=9.8, far=False)
    return s


def b_turboprop_single():
    # Single Turboprop (Pilatus PC-12 / TBM 940): long pointed turbine nose, prop, sleek T-tail!
    s = Sil()
    L, h, fin_top = 100.0, 12.6, 28.8

    body = []
    body += cubic_bezier((95.5, h * 0.44), (95.5, h * 0.64), (88.0, h * 0.74), (77.0, h * 0.78), n=12)
    body += cubic_bezier((77.0, h * 0.78), (72.0, h * 1.00), (68.0, h * 1.04), (62.0, h * 1.04), n=10)
    body.append((38.0, h * 1.00))
    body += cubic_bezier((38.0, h * 1.00), (29.0, h * 0.92), (24.0, h * 0.80), (20.5, h * 0.80), n=10)
    body += cubic_bezier((20.5, h * 0.80), (16.5, h * 0.95), (13.0, h * 1.35), (7.6, fin_top - 0.5), n=12)
    body += quad_bezier((7.6, fin_top - 0.5), (6.9, fin_top), (5.8, fin_top), n=5)
    body.append((2.0, fin_top))
    body += quad_bezier((2.0, fin_top), (1.4, fin_top), (1.3, fin_top - 0.5), n=5)
    body.append((5.8, h * 0.66))
    body += cubic_bezier((5.8, h * 0.66), (3.0, h * 0.62), (1.0, h * 0.58), (0.0, h * 0.54), n=6)
    body.append((0.0, h * 0.42))
    body += cubic_bezier((0.0, h * 0.42), (16.0, h * 0.24), (32.0, 0.0), (44.0, 0.0), n=12)
    body.append((79.0, 0.0))
    body += cubic_bezier((79.0, 0.0), (90.0, h * 0.06), (95.5, h * 0.18), (95.5, h * 0.44), n=12)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(4.0, fin_top), (7.6, h * 0.66)], width=0.95)
    s.stroke([(77.5, h * 0.78), (77.5, h * 0.04)], width=0.9)
    # Turbine exhaust stack detail on nose side
    s.solid(rounded_rect(83.0, h * 0.28, 86.5, h * 0.44, r=0.4, n=3), min_w=300)

    # Cockpit + 5 crisp square-rounded executive cabin windows + rear cargo door seam
    s.solid([(76.0, h * 0.62), (71.5, h * 0.94), (65.5, h * 0.94), (65.5, h * 0.60)])
    draw_window_row(s, 39.5, 62.0, h * 0.70, w=2.5, h=2.4, step=5.2)
    draw_door(s, 33.5, h * 0.22, h * 0.84, w=4.5, min_w=300)

    # Swept T-tail
    s.panel(ellipse(5.2, fin_top - 0.9, 5.0, 1.3, n=16), fill=0, width=1.2)
    draw_horiz_stab(s, 8.8, -5.0, fin_top - 0.9, thick=1.8)
    draw_wing_root(s, x_le=68.5, x_te=44.5, y_root=h * 0.12, thick=2.2,
                   fairing_le=72.0, fairing_te=40.0, belly_drop=1.0)

    # Nose propeller & conical spinner
    px, yc, pr = 97.2, h * 0.42, 11.8
    s.panel([(px - 0.45, yc - pr), (px + 0.45, yc - pr), (px + 0.75, yc),
             (px + 0.45, yc + pr), (px - 0.45, yc + pr), (px - 0.75, yc)], fill=1, width=0.85)
    s.panel([(95.2, yc + 2.2), (97.8, yc + 1.6), (100.8, yc), (97.8, yc - 1.6), (95.2, yc - 2.2)],
            fill=0, width=1.2)
    return s


def b_twin_turboprop():
    # Beechcraft King Air 350: sleek pointed nose, swept T-tail, round cabin windows, wing PT6A nacelle!
    def add_pt6(s, h):
        draw_turboprop(s, x_front=75.0, length=23.5, yc=0.6, h=4.8, prop_r=9.8, far=False)

    return tech_airliner(
        h=11.6, fin_top=27.8, nose="douglas", tail="pointed", t_tail=True,
        fin_root_x=24.5, fin_le_tip_x=7.8, fin_te_tip_x=2.4, fin_te_base_x=6.8,
        engines="none", wing_le=67.0, wing_te=43.0, belly_drop=1.0, tail_upsweep_x=34.0,
        win_start=40.0, win_end=75.0, win_y=0.62, win_w=2.3, win_h=2.3, win_step=5.8, win_oval=True,
        doors=[(33.5, False)], extra_fn=add_pt6,
    )


# ---- Rotorcraft, Glider, Balloon & UAV -------------------------------------


def b_helicopter():
    # Modern Executive/Utility Helicopter (Airbus H135 / Bell 407): teardrop cabin, fenestron tail, skids!
    s = Sil()
    h = 18.0

    # 1. Tapered tail boom + swept vertical fin with shrouded Fenestron tail rotor!
    boom = (
        [(54.0, h * 0.64), (10.5, h * 0.60)]
        + [(5.5, h * 1.28), (1.0, h * 1.28), (3.5, h * 0.56), (2.2, h * 0.14), (5.5, h * 0.14), (10.0, h * 0.46)]
        + [(54.0, h * 0.36)]
    )
    s.panel(boom, fill=0, width=1.5)
    # Shrouded Fenestron tail rotor duct & hub
    s.panel(ellipse(7.5, h * 0.66, 3.8, 3.8, n=20), fill=0, width=1.3)
    s.panel(ellipse(7.5, h * 0.66, 2.5, 2.5, n=18), fill=0, width=0.95)
    s.solid(ellipse(7.5, h * 0.66, 0.85, 0.85, n=10))
    # Horizontal stabilizer with small endplate on boom
    draw_horiz_stab(s, 23.0, 15.0, h * 0.52, thick=1.8, elevator=False)

    # 2. Aerodynamic teardrop cabin pod + dorsal turbine cowling
    cabin = []
    cabin += cubic_bezier((97.0, h * 0.34), (97.0, h * 0.68), (89.0, h * 0.94), (77.0, h * 1.00), n=14)
    cabin += cubic_bezier((77.0, h * 1.00), (72.0, h * 1.18), (66.0, h * 1.22), (55.0, h * 1.16), n=12)
    # Turbine exhaust nozzle at rear of cowling
    cabin += [(50.5, h * 1.04), (50.5, h * 0.86), (52.5, h * 0.82)]
    cabin += cubic_bezier((52.5, h * 0.82), (48.0, h * 0.55), (49.0, h * 0.14), (57.0, 0.0), n=12)
    cabin.append((84.0, 0.0))
    cabin += cubic_bezier((84.0, 0.0), (93.5, h * 0.04), (97.0, h * 0.14), (97.0, h * 0.34), n=12)
    s.panel(cabin, fill=0, width=1.6)

    # Cowling separation seam + dark exhaust mouth
    s.stroke(cubic_bezier((77.0, h * 0.96), (66.0, h * 0.94), (58.0, h * 0.90), (52.5, h * 0.82), n=10), width=0.9)
    s.solid([(51.8, h * 1.01), (50.5, h * 1.00), (50.5, h * 0.88), (51.8, h * 0.88)])

    # Panoramic cockpit windscreen + lower chin window + cabin side door window
    s.solid(
        cubic_bezier((94.8, h * 0.44), (93.5, h * 0.68), (88.0, h * 0.86), (79.5, h * 0.88), n=10)
        + [(79.5, h * 0.44), (94.8, h * 0.44)]
    )
    s.solid([(77.5, h * 0.84), (77.5, h * 0.44), (66.5, h * 0.44), (66.5, h * 0.84)])
    s.solid([(64.5, h * 0.80), (64.5, h * 0.46), (58.0, h * 0.46), (58.0, h * 0.78)])
    draw_door(s, 72.0, h * 0.12, h * 0.88, w=12.0, min_w=300)

    # 3. Tubular landing skids with angled cross-struts
    s.stroke([(82.0, 0.2), (83.5, -5.8)], width=1.35)
    s.stroke([(61.0, 0.2), (59.5, -5.8)], width=1.35)
    s.stroke([(52.0, -5.8), (89.5, -5.8), (93.5, -3.8)], width=1.55)

    # 4. Rotor mast, swashplate hub & slightly drooping main rotor blades!
    mx = 69.0
    s.panel([(mx - 1.8, h * 1.18), (mx + 1.8, h * 1.18), (mx + 1.4, h * 1.44), (mx - 1.4, h * 1.44)],
            fill=0, width=1.3)
    s.panel(ellipse(mx, h * 1.48, 3.2, 1.3, n=14), fill=0, width=1.2)
    # Main rotor blades spanning cleanly across top
    s.panel([
        (20.0, h * 1.46), (mx - 3.0, h * 1.52), (mx + 3.0, h * 1.52), (100.0, h * 1.46),
        (100.0, h * 1.41), (mx + 3.0, h * 1.45), (mx - 3.0, h * 1.45), (20.0, h * 1.41),
    ], fill=1, width=0.85)
    return s


def b_chinook():
    # Boeing CH-47 Chinook: iconic tandem-rotor heavy lift helicopter!
    s = Sil()
    h = 14.5

    # Main slab-sided fuselage + raised forward & taller aft rotor pylons
    body = []
    # Glazed nose & cockpit brow rising into forward rotor pylon!
    body += cubic_bezier((98.0, h * 0.42), (98.0, h * 0.72), (94.0, h * 0.96), (88.0, h * 1.06), n=12)
    body += [(84.0, h * 1.38), (78.0, h * 1.38), (74.0, h * 1.06)]
    # Flat dorsal tunnel roof back to tall aft rotor pylon
    body.append((28.0, h * 1.06))
    body += [(22.0, h * 1.88), (11.5, h * 1.88), (9.5, h * 1.05)]
    # Rear cargo ramp lower hull
    body += cubic_bezier((9.5, h * 1.05), (9.0, h * 0.56), (13.0, h * 0.24), (24.0, 0.0), n=12)
    body.append((88.0, 0.0))
    body += cubic_bezier((88.0, 0.0), (95.5, h * 0.05), (98.0, h * 0.18), (98.0, h * 0.42), n=12)
    s.panel(body, fill=0, width=1.6)

    # Rear cargo ramp seam + dorsal tunnel crease
    s.stroke([(23.0, 0.2), (18.0, h * 0.55), (10.0, h * 0.65)], width=0.9)
    s.stroke([(27.0, h * 0.96), (75.0, h * 0.96)], width=0.85, min_w=300)

    # Cockpit glazing + crew door + classic row of circular Chinook troop portholes
    s.solid([(96.2, h * 0.48), (93.8, h * 0.84), (88.5, h * 0.86), (88.5, h * 0.48)])
    s.solid([(87.0, h * 0.48), (87.0, h * 0.80), (83.5, h * 0.80), (83.5, h * 0.48)])
    draw_door(s, 77.5, h * 0.16, h * 0.82, w=4.2)
    for wx in (68.0, 60.5, 53.0, 45.5, 38.0):
        s.solid(ellipse(wx, h * 0.60, 1.15, 1.15, n=12))

    # Long lower external fuel sponson running along side of hull
    sponson = rounded_rect(26.0, -0.4, 73.5, h * 0.34, r=1.8, n=5)
    s.panel(sponson, fill=0, width=1.35)

    # Rear side-mounted T55 turboshaft engine pod on aft pylon!
    draw_turbofan(s, x_front=30.0, length=15.0, yc=h * 1.16, h=4.8, slim=True)

    # Fixed tandem landing gear wheels (forward + aft struts)
    draw_fixed_wheel(s, 81.0, -4.6, 0.2, r=2.6, pant=False)
    draw_fixed_wheel(s, 29.0, -4.6, 0.2, r=2.6, pant=False, strut_dx=-1.2)

    # Forward & aft rotor masts + overlapping tandem rotor discs!
    for mx, my, span in ((81.0, h * 1.40, 58.0), (17.0, h * 1.90, 58.0)):
        s.panel([(mx - 1.5, my - 0.5), (mx + 1.5, my - 0.5), (mx + 1.2, my + 2.4), (mx - 1.2, my + 2.4)],
                fill=0, width=1.2)
        s.panel([
            (mx - span / 2, my + 2.5), (mx - 2.0, my + 3.0), (mx + 2.0, my + 3.0), (mx + span / 2, my + 2.5),
            (mx + span / 2, my + 2.0), (mx + 2.0, my + 2.3), (mx - 2.0, my + 2.3), (mx - span / 2, my + 2.0),
        ], fill=1, width=0.85)
    return s


def b_glider():
    # Sleek Sailplane / Glider: teardrop cockpit pod, flush canopy, needle tailboom, T-tail!
    s = Sil()
    L, h, fin_top = 100.0, 7.6, 20.5

    body = []
    # Smooth aerodynamic nose rising into flush canopy line
    body += cubic_bezier((L, h * 0.34), (L, h * 0.68), (L - 5.5, h * 1.05), (L - 16.0, h * 1.08), n=16)
    # Smooth taper behind cockpit into ultra-thin sailplane tailboom
    body += cubic_bezier((L - 16.0, h * 1.08), (L - 28.0, h * 1.06), (L - 36.0, h * 0.62), (52.0, h * 0.54), n=14)
    body.append((13.5, h * 0.48))
    # Tall slim vertical tail fin
    body += cubic_bezier((13.5, h * 0.48), (11.5, h * 0.62), (9.8, h * 1.10), (7.6, fin_top - 0.4), n=10)
    body += quad_bezier((7.6, fin_top - 0.4), (7.0, fin_top), (6.0, fin_top), n=4)
    body.append((3.2, fin_top))
    body.append((5.4, h * 0.28))
    # Underside of needle boom back forward to cockpit belly
    body.append((54.0, h * 0.20))
    body += cubic_bezier((54.0, h * 0.20), (66.0, h * 0.02), (76.0, 0.0), (88.0, 0.0), n=12)
    body += cubic_bezier((88.0, 0.0), (96.0, h * 0.02), (L, h * 0.12), (L, h * 0.34), n=12)
    s.panel(body, fill=0, width=1.6)

    # Rudder hinge line
    s.stroke([(4.8, fin_top), (6.6, h * 0.30)], width=0.9)
    # Sleek flush bubble canopy glazing
    s.solid(
        cubic_bezier((94.0, h * 0.56), (90.0, h * 0.88), (83.0, h * 0.96), (74.5, h * 0.92), n=12)
        + [(74.5, h * 0.56), (94.0, h * 0.56)]
    )
    # T-tail horizontal stabilizer
    draw_horiz_stab(s, 10.0, -1.5, fin_top - 0.4, thick=1.35)
    # Thin shoulder-mounted wing root airfoil
    draw_wing_root(s, x_le=73.5, x_te=62.0, y_root=h * 0.64, thick=1.6)
    return s


def b_balloon():
    # Hot Air Balloon: graceful teardrop envelope with curved longitudinal gore ribs, burner & wicker basket!
    s = Sil()
    cx = 50.0
    # Teardrop envelope outline
    env = (
        cubic_bezier((cx, 94.0), (cx + 22.0, 94.0), (cx + 31.0, 77.0), (cx + 30.0, 62.0), n=14)
        + cubic_bezier((cx + 30.0, 62.0), (cx + 29.0, 46.0), (cx + 14.0, 32.0), (cx + 9.5, 22.0), n=14)
        + [(cx - 9.5, 22.0)]
        + cubic_bezier((cx - 9.5, 22.0), (cx - 14.0, 32.0), (cx - 29.0, 46.0), (cx - 30.0, 62.0), n=14)
        + cubic_bezier((cx - 30.0, 62.0), (cx - 31.0, 77.0), (cx - 22.0, 94.0), (cx, 94.0), n=14)
    )
    s.panel(env, fill=0, width=1.6)

    # Graceful curved meridian gore panel lines (true 3D spherical projection!)
    for f in (-0.62, -0.26, 0.26, 0.62):
        gore = (
            cubic_bezier((cx, 94.0), (cx + 22.0 * f, 94.0), (cx + 30.5 * f, 77.0), (cx + 29.5 * f, 62.0), n=12)
            + cubic_bezier((cx + 29.5 * f, 62.0), (cx + 28.5 * f, 46.0), (cx + 13.5 * f, 32.0), (cx + 9.2 * f, 22.0), n=12)
        )
        s.stroke(gore, width=0.95)
    # Horizontal equator decorative band
    s.stroke([(cx - 29.5, 62.0), (cx + 29.5, 62.0)], width=0.9)
    s.stroke([(cx - 28.0, 54.0), (cx + 28.0, 54.0)], width=0.85, min_w=300)

    # Skirt throat & suspension cables to basket
    s.panel([(cx - 8.5, 22.0), (cx + 8.5, 22.0), (cx + 7.2, 18.5), (cx - 7.2, 18.5)], fill=0, width=1.2)
    s.stroke([(cx - 6.8, 18.5), (cx - 4.8, 10.5)], width=1.0)
    s.stroke([(cx + 6.8, 18.5), (cx + 4.8, 10.5)], width=1.0)
    s.stroke([(cx, 18.5), (cx, 10.5)], width=0.9, min_w=300)
    # Burner detail
    s.solid(rect(cx - 2.2, 14.5, cx + 2.2, 16.5))

    # Wicker basket with weave detail
    s.panel(rounded_rect(cx - 5.5, 1.5, cx + 5.5, 10.5, r=1.0, n=3), fill=0, width=1.45)
    s.stroke([(cx - 5.2, 7.5), (cx + 5.2, 7.5)], width=0.85, min_w=300)
    s.stroke([(cx - 5.2, 4.5), (cx + 5.2, 4.5)], width=0.85, min_w=300)
    return s


def b_uav():
    # HALE Drone (RQ-4 Global Hawk): smooth bulbous satcom nose, dorsal turbofan pod, canted V-tail!
    s = Sil()
    L, h = 100.0, 9.0

    # Far V-tail fin
    s.panel([(16.0, h * 0.55), (7.5, h * 0.55), (1.0, h * 2.15), (6.0, h * 2.15)], fill=0, width=1.15)

    # Sleek bulbous satcom-dome nose + slender fuselage (no cockpit windows!)
    fus = []
    fus += cubic_bezier((L, h * 0.42), (L, h * 0.85), (L - 6.0, h * 1.42), (L - 18.5, h * 1.46), n=16)
    fus += cubic_bezier((L - 18.5, h * 1.46), (L - 30.0, h * 1.44), (L - 38.0, h * 1.00), (54.0, h * 0.92), n=14)
    fus.append((6.5, h * 0.68))
    fus.append((6.5, h * 0.36))
    fus += cubic_bezier((6.5, h * 0.36), (26.0, h * 0.14), (48.0, 0.0), (80.0, 0.0), n=14)
    fus += cubic_bezier((80.0, 0.0), (93.0, 0.0), (L, h * 0.12), (L, h * 0.42), n=12)
    s.panel(fus, fill=0, width=1.6)

    # Satcom dome seam & underbelly EO/IR sensor turret
    s.stroke([(74.0, h * 1.38), (74.0, 0.2)], width=0.9)
    s.panel(ellipse(76.5, -1.2, 2.5, 1.8, n=12), fill=0, width=1.15)
    s.solid(ellipse(77.8, -1.4, 0.7, 0.7, n=8))

    # Dorsal pod-mounted turbofan engine (Global Hawk signature!)
    draw_turbofan(s, x_front=44.5, length=31.0, yc=h * 1.18, h=5.6, slim=True)

    # Near canted V-tail fin
    s.panel([(18.5, h * 0.50), (9.5, h * 0.50), (3.0, h * 2.22), (8.2, h * 2.22)], fill=0, width=1.45)
    s.stroke([(5.2, h * 2.22), (11.5, h * 0.52)], width=0.9)

    # Slim high-aspect wing root airfoil
    draw_wing_root(s, x_le=62.0, x_te=44.0, y_root=h * 0.20, thick=1.8)
    return s


# ---- Fighters, Trainers & Military Special Mission / Bombers ---------------


def b_fighter():
    # Modern Air-Superiority Fighter (F-16 / F-22 / Rafale silhouette): needle nose, bubble canopy,
    # intake, swept vertical fin, low stabilator, afterburner exhaust nozzle!
    s = Sil()
    L, h, fin_top = 100.0, 8.2, 23.0

    # Unified sleek supersonic airframe + swept vertical stabilizer
    body = []
    # Razor-sharp radar nose cone rising to windscreen base
    body += cubic_bezier((L, h * 0.36), (L - 6.0, h * 0.58), (L - 14.0, h * 0.80), (L - 22.0, h * 0.90), n=14)
    # Bubble canopy contour rising smoothly above nose
    body += cubic_bezier((L - 22.0, h * 0.90), (L - 27.0, h * 1.56), (L - 38.0, h * 1.58), (L - 44.0, h * 1.05), n=16)
    # Dorsal spine back to swept vertical tail leading edge
    body.append((32.5, h * 0.96))
    body += cubic_bezier((32.5, h * 0.96), (27.5, h * 1.15), (22.0, h * 1.70), (14.5, fin_top - 0.3), n=12)
    body.append((8.2, fin_top))
    body.append((10.5, h * 0.88))
    # Rear fuselage deck to exhaust nozzle
    body.append((6.0, h * 0.82))
    body.append((6.0, h * 0.16))
    # Lower fuselage belly + underslung chin intake (F-16/Eurofighter style!)
    body.append((44.0, 0.0))
    body.append((54.0, -2.6))
    body.append((72.5, -2.6))
    body.append((74.8, h * 0.06))
    body += cubic_bezier((74.8, h * 0.06), (86.0, h * 0.08), (94.0, h * 0.18), (L, h * 0.36), n=12)
    s.panel(body, fill=0, width=1.6)

    # Radome seam + rudder hinge line + intake mouth shadow
    s.stroke([(82.0, h * 0.78), (82.0, h * 0.08)], width=0.9)
    s.stroke([(10.4, fin_top), (12.4, h * 0.88)], width=0.95)
    s.solid([(74.5, 0.0), (72.5, 0.0), (71.2, -2.4), (72.8, -2.4)])

    # Bubble canopy glazing (clear framed technical glass with dark interior tint + bow frame)
    canopy_glass = (
        cubic_bezier((76.5, h * 0.94), (72.0, h * 1.44), (62.5, h * 1.46), (57.5, h * 1.02), n=14)
        + [(57.5, h * 0.90), (76.5, h * 0.90)]
    )
    s.solid(canopy_glass)
    # Crisp white canopy bow highlight on large view
    s.stroke([(67.2, h * 1.42), (66.5, h * 0.90)], width=0.85, color=0, min_w=300)

    # Afterburner turkey-feather exhaust nozzle
    s.panel([(6.0, h * 0.78), (0.0, h * 0.68), (0.0, h * 0.28), (6.0, h * 0.18)], fill=0, width=1.35)
    s.stroke([(3.0, h * 0.72), (3.0, h * 0.24)], width=0.85, min_w=300)
    s.solid([(1.5, h * 0.62), (0.0, h * 0.60), (0.0, h * 0.36), (1.5, h * 0.34)])

    # Low horizontal stabilator & thin swept wing profile
    draw_horiz_stab(s, 17.5, 2.5, h * 0.42, thick=1.8, elevator=False)
    s.panel([(62.0, h * 0.36), (26.0, h * 0.36), (22.0, -0.3), (56.0, -0.3)], fill=0, width=1.25)
    return s


def b_jet_trainer():
    # Tandem Jet Trainer (BAE Hawk / M-346 / T-38): long 2-seat bubble canopy, side intake, swept fin!
    s = Sil()
    L, h, fin_top = 100.0, 10.2, 25.4

    body = []
    body += cubic_bezier((L, h * 0.34), (L - 4.5, h * 0.60), (L - 12.0, h * 0.84), (L - 20.0, h * 0.92), n=14)
    # Long raised tandem two-seat canopy!
    body += cubic_bezier((L - 20.0, h * 0.92), (L - 26.0, h * 1.56), (L - 48.0, h * 1.58), (L - 56.0, h * 0.98), n=18)
    body.append((28.5, h * 0.92))
    body += cubic_bezier((28.5, h * 0.92), (24.0, h * 1.12), (19.0, h * 1.62), (11.8, fin_top - 0.4), n=12)
    body += quad_bezier((11.8, fin_top - 0.4), (11.0, fin_top), (9.8, fin_top), n=4)
    body.append((5.4, fin_top))
    body.append((7.8, h * 0.80))
    body.append((3.0, h * 0.72))
    body.append((3.0, h * 0.24))
    body += cubic_bezier((3.0, h * 0.24), (20.0, h * 0.08), (38.0, 0.0), (80.0, 0.0), n=12)
    body += cubic_bezier((80.0, 0.0), (92.0, h * 0.04), (L, h * 0.14), (L, h * 0.34), n=12)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(7.4, fin_top), (9.6, h * 0.80)], width=0.95)
    s.stroke([(84.5, h * 0.78), (84.5, h * 0.06)], width=0.85)

    # Tandem canopy glass + center arch separating front & rear cockpits!
    glass = (
        cubic_bezier((78.5, h * 0.94), (73.0, h * 1.46), (53.0, h * 1.48), (46.0, h * 0.96), n=16)
        + [(46.0, h * 0.86), (78.5, h * 0.86)]
    )
    s.solid(glass)
    s.stroke([(63.5, h * 1.46), (62.5, h * 0.86)], width=1.3, color=0)

    # Rear exhaust nozzle
    s.panel([(3.0, h * 0.68), (0.0, h * 0.62), (0.0, h * 0.32), (3.0, h * 0.26)], fill=0, width=1.2)
    # Side air intake scoop + low wing root + stabilator
    draw_horiz_stab(s, 17.5, 1.0, h * 0.48, thick=1.8, elevator=False)
    s.panel(rounded_rect(42.0, h * 0.32, 60.5, h * 0.76, r=1.4, n=4), fill=0, width=1.25)
    s.solid(rect(59.2, h * 0.36, 60.4, h * 0.72))
    draw_wing_root(s, x_le=62.5, x_te=41.5, y_root=h * 0.12, thick=1.9)
    return s


def b_prop_trainer():
    # Turboprop Trainer (Pilatus PC-21 / T-6 Texan II): long turbine nose, prop, tandem bubble canopy!
    s = Sil()
    L, h, fin_top = 100.0, 11.4, 27.4

    body = []
    body += cubic_bezier((95.5, h * 0.44), (95.5, h * 0.64), (88.0, h * 0.76), (76.0, h * 0.86), n=12)
    # Long raised tandem bubble canopy (stepped rear instructor seat)
    body += cubic_bezier((76.0, h * 0.86), (70.0, h * 1.48), (50.0, h * 1.54), (41.0, h * 0.96), n=18)
    body.append((25.0, h * 0.88))
    body += cubic_bezier((25.0, h * 0.88), (20.5, h * 1.04), (16.5, h * 1.45), (10.5, fin_top - 0.4), n=12)
    body += quad_bezier((10.5, fin_top - 0.4), (9.8, fin_top), (8.6, fin_top), n=4)
    body.append((4.2, fin_top))
    body.append((6.0, h * 0.72))
    body += cubic_bezier((6.0, h * 0.72), (3.0, h * 0.66), (1.0, h * 0.60), (0.0, h * 0.56), n=6)
    body.append((0.0, h * 0.42))
    body += cubic_bezier((0.0, h * 0.42), (18.0, h * 0.22), (36.0, 0.0), (50.0, 0.0), n=12)
    body.append((78.0, 0.0))
    body += cubic_bezier((78.0, 0.0), (90.0, h * 0.06), (95.5, h * 0.18), (95.5, h * 0.44), n=12)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(6.2, fin_top), (7.8, h * 0.72)], width=0.95)
    s.stroke([(77.5, h * 0.84), (77.5, h * 0.04)], width=0.9)
    # Exhaust stub on cowling
    s.solid(rounded_rect(82.0, h * 0.30, 85.5, h * 0.45, r=0.4, n=3), min_w=300)

    # Tandem canopy glass + center arch
    glass = (
        cubic_bezier((74.5, h * 0.88), (69.0, h * 1.40), (51.0, h * 1.44), (43.0, h * 0.94), n=16)
        + [(43.0, h * 0.84), (74.5, h * 0.84)]
    )
    s.solid(glass)
    s.stroke([(59.5, h * 1.42), (58.5, h * 0.84)], width=1.3, color=0)

    draw_horiz_stab(s, 15.5, -1.0, h * 0.54, thick=1.8)
    draw_wing_root(s, x_le=70.5, x_te=48.0, y_root=h * 0.12, thick=2.1)

    px, yc, pr = 97.2, h * 0.44, 12.2
    s.panel([(px - 0.45, yc - pr), (px + 0.45, yc - pr), (px + 0.75, yc),
             (px + 0.45, yc + pr), (px - 0.45, yc + pr), (px - 0.75, yc)], fill=1, width=0.85)
    s.panel([(95.2, yc + 2.2), (97.8, yc + 1.6), (100.8, yc), (97.8, yc - 1.6), (95.2, yc - 2.2)],
            fill=0, width=1.2)
    return s


def b_a400m():
    # Airbus A400M Atlas: high wing, tall swept T-tail, refueling probe, 4 big TP400 turboprops!
    return tech_high_winger(
        h=12.4, fin_top=29.2, nose="round", t_tail=True, upsweep=True, probe=True,
        fin_root_x=26.0, fin_le_tip_x=10.5, fin_te_tip_x=3.8, fin_te_base_x=6.8,
        engines="tp4", eng_x=66.5, eng_len=14.8, eng_h=4.8, prop_r=9.0,
        sponson=True, sponson_x=(40.0, 67.0), portholes=True,
    )


def b_c130():
    # Lockheed C-130 Hercules: iconic huge tall upright fin, LOW tailplane, 4 turboprops!
    return tech_high_winger(
        h=12.2, fin_top=30.2, nose="round", t_tail=False, upsweep=True,
        fin_root_x=24.0, fin_le_tip_x=12.2, fin_te_tip_x=5.0, fin_te_base_x=6.0,
        engines="tp4", eng_x=64.5, eng_len=13.5, eng_h=4.6, prop_r=8.4,
        sponson=True, sponson_x=(40.0, 66.0), portholes=True,
    )


def b_c17():
    # Boeing C-17 Globemaster III: muscular widebody lifter, tall swept T-tail, 4 large F117 turbofans!
    return tech_high_winger(
        h=13.4, fin_top=30.0, nose="round", t_tail=True, upsweep=True,
        fin_root_x=27.0, fin_le_tip_x=9.4, fin_te_tip_x=3.2, fin_te_base_x=7.0,
        engines="jet4", eng_x=65.5, eng_len=13.4, eng_h=6.0,
        sponson=True, sponson_x=(40.0, 68.0), portholes=True,
    )


def b_c5():
    # Lockheed C-5 Galaxy: colossal visor-nose lifter with iconic giant swept T-tail + 4 turbofans!
    return tech_high_winger(
        h=12.2, fin_top=29.2, nose="visor", t_tail=True, upsweep=True,
        fin_root_x=26.5, fin_le_tip_x=8.8, fin_te_tip_x=2.8, fin_te_base_x=6.8,
        engines="jet4", eng_x=65.5, eng_len=12.8, eng_h=5.6,
        sponson=True, sponson_x=(39.0, 67.0), portholes=True,
    )


def _kc135_base(boom=True, rotodome=False):
    """Boeing 707 / KC-135 Stratotanker & E-3 Sentry AWACS airframe."""
    def extras(s, h):
        # Iconic 707/KC-135 HF tail-tip probe antenna
        s.stroke([(9.5, 25.8), (15.0, 25.8)], width=1.0)
        if boom:
            # Extended flying refueling boom + ruddevator control vanes!
            s.stroke([(20.0, 1.4), (7.5, -5.8)], width=1.35)
            s.panel([(13.8, -2.0), (10.2, -3.4), (9.5, -4.4), (13.0, -3.0)], fill=0, width=1.0)
        if rotodome:
            # E-3 Sentry AWACS twin swept pylons & massive rotating radar rotodome!
            s.panel([(38.5, h - 0.2), (35.5, h - 0.2), (31.5, h + 6.4), (34.5, h + 6.4)], fill=0, width=1.2)
            s.panel([(31.5, h - 0.2), (28.5, h - 0.2), (25.5, h + 6.4), (28.5, h + 6.4)], fill=0, width=1.2)
            dome = ellipse(31.0, h + 7.2, 16.5, 2.3, n=32)
            s.panel(dome, fill=0, width=1.5)
            # Signature radar antenna strip across center of rotodome
            s.solid(rect(24.0, h + 6.4, 38.0, h + 8.0))

    return tech_airliner(
        h=9.8, fin_top=26.2, nose="boeing",
        fin_root_x=24.5, fin_le_tip_x=9.2, fin_te_tip_x=3.8, fin_te_base_x=6.4,
        engines="under4", eng_x=65.5, eng_len=11.4, eng_h=4.6, eng_yc=-2.1,
        far_eng_x=51.0, far_eng_scale=0.88,
        win_start=80.0, win_end=79.0,  # windowless military fuselage
        doors=[(85.5, False)], extra_fn=extras,
    )


def b_kc135():
    return _kc135_base(boom=True, rotodome=False)


def b_awacs():
    return _kc135_base(boom=False, rotodome=True)


def b_mil_twinprop():
    # Military Twin Turboprop (Airbus C-295 / Leonardo C-27J): high wing, conventional tail, 2 props!
    return tech_high_winger(
        h=11.4, fin_top=27.8, nose="round", t_tail=False, upsweep=True,
        fin_root_x=26.0, fin_le_tip_x=10.8, fin_te_tip_x=4.4, fin_te_base_x=6.5,
        engines="tp2", eng_x=66.0, eng_len=15.5, eng_h=4.8, prop_r=8.6,
        sponson=True, sponson_x=(40.0, 66.0), portholes=True,
    )


def b_b52():
    # Boeing B-52 Stratofortress: ultra-slender long fuselage, giant tall upright clipped fin,
    # paired twin-engine pods slung forward on angled pylons!
    s = Sil()
    L, h, fin_top = 100.0, 7.6, 26.8

    # Far outboard paired twin-engine pod
    draw_turbofan(s, x_front=51.5, length=12.5, yc=-2.6, h=3.6,
                  pylon_pts=[(49.0, h * 0.8), (46.0, h * 0.8), (43.5, -1.0), (46.5, -1.0)],
                  far=True, slim=True)

    # Unified slender B-52 fuselage + iconic massive tall vertical tail!
    body = []
    body += cubic_bezier((L, h * 0.48), (L, h * 0.78), (L - 2.6, h * 0.96), (L - 7.5, h), n=12)
    body.append((24.0, h))
    # Tall classic clipped B-52H vertical fin
    body.append((17.5, fin_top - 0.5))
    body += quad_bezier((17.5, fin_top - 0.5), (16.8, fin_top), (15.5, fin_top), n=5)
    body.append((9.2, fin_top))
    body.append((9.2, h * 0.92))
    # Short squared-off rear gunner/ECM tailcone
    body.append((3.5, h * 0.88))
    body.append((3.5, h * 0.36))
    body += cubic_bezier((3.5, h * 0.36), (12.0, h * 0.16), (22.0, 0.0), (34.0, 0.0), n=10)
    body.append((L - 8.5, 0.0))
    body += cubic_bezier((L - 8.5, 0.0), (L - 3.0, h * 0.04), (L, h * 0.18), (L, h * 0.48), n=12)
    s.panel(body, fill=0, width=1.6)

    # Rudder hinge + radome seam + signature chin electro-optical sensor blisters!
    s.stroke([(11.4, fin_top), (11.4, h * 0.92)], width=0.95)
    s.stroke([(L - 3.5, h * 0.82), (L - 3.5, h * 0.14)], width=0.85)
    s.panel(ellipse(L - 10.5, -0.5, 2.8, 1.1, n=12), fill=0, width=1.0, min_w=300)

    # B-52 stepped cockpit glazing
    draw_cockpit_2pane(s, L, h, dx=4.5, y0=0.60, y1=0.84, w1=3.2, w2=2.2, slant=1.0)

    draw_horiz_stab(s, 14.5, -1.0, h * 0.62, thick=1.6)
    draw_high_wing_box(s, x_le=68.0, x_te=40.0, y_top=h * 0.96, thick=1.8)

    # Near inboard paired twin-engine pod (slung forward on sleek angled pylon!)
    pylon = [(64.0, h * 0.94), (60.0, h * 0.94), (55.5, -1.8), (59.5, -1.8)]
    draw_turbofan(s, x_front=66.5, length=13.5, yc=-3.4, h=4.0, pylon_pts=pylon, far=False, slim=True)
    return s


def b_b1():
    # Rockwell B-1B Lancer: razor-sleek supersonic bomber, curving nose + SMCS canard vane,
    # cruciform swept tail, underslung boxy twin F101 engine nacelles!
    s = Sil()
    L, h, fin_top = 100.0, 7.6, 22.8

    body = []
    # Long graceful shark-like supersonic nose
    body += cubic_bezier((L, h * 0.24), (L - 6.0, h * 0.48), (L - 14.0, h * 0.88), (L - 25.0, h * 1.04), n=16)
    body.append((30.0, h * 0.96))
    # Tall steeply swept vertical tail
    body += cubic_bezier((30.0, h * 0.96), (24.5, h * 1.15), (18.5, h * 1.75), (10.5, fin_top - 0.4), n=14)
    body += quad_bezier((10.5, fin_top - 0.4), (9.8, fin_top), (8.6, fin_top), n=4)
    body.append((4.2, fin_top))
    body.append((8.5, h * 0.82))
    body.append((2.0, h * 0.62))
    body.append((2.0, h * 0.26))
    body += cubic_bezier((2.0, h * 0.26), (14.0, h * 0.10), (26.0, 0.0), (80.0, 0.0), n=12)
    body += cubic_bezier((80.0, 0.0), (92.0, h * 0.02), (96.0, h * 0.08), (L, h * 0.24), n=12)
    s.panel(body, fill=0, width=1.6)

    s.stroke([(6.4, fin_top), (10.5, h * 0.82)], width=0.95)
    # Iconic forward SMCS ride-control canard vane on the nose!
    s.panel([(88.5, h * 0.34), (84.0, h * 0.34), (82.5, h * 0.12), (86.5, h * 0.12)], fill=0, width=1.0)
    # Sleek blended cockpit glazing
    s.solid([(84.5, h * 0.64), (81.5, h * 0.86), (76.5, h * 0.88), (76.5, h * 0.64)])
    s.solid([(75.8, h * 0.64), (75.8, h * 0.88), (73.0, h * 0.85), (73.0, h * 0.64)])

    # Signature B-1 cruciform horizontal stabilator mounted halfway up the vertical tail!
    draw_horiz_stab(s, 20.0, 4.0, h * 1.72, thick=1.7, elevator=False)

    # Underslung boxy twin F101 engine nacelles + variable exhaust nozzles
    s.panel([(45.0, 0.5), (16.0, 0.5), (14.5, -3.6), (42.0, -3.6)], fill=0, width=1.35)
    s.solid([(44.5, 0.2), (43.0, 0.2), (41.5, -3.3), (42.8, -3.3)])
    s.panel([(16.0, 0.2), (10.5, -0.4), (10.5, -3.2), (14.8, -3.5)], fill=0, width=1.2)
    s.solid([(12.2, -0.7), (10.5, -0.8), (10.5, -2.8), (12.2, -2.9)])

    # Blended variable-sweep wing root
    s.panel([(68.0, h * 0.36), (34.0, h * 0.36), (31.0, h * 0.08), (64.0, h * 0.08)], fill=0, width=1.2)
    return s


def b_b2():
    # Northrop B-2 Spirit: ultra-sleek stealth flying wing side profile!
    s = Sil()
    L = 100.0
    # Smooth razor-thin flying-wing airfoil + central faceted stealth cockpit hump
    body = []
    body += cubic_bezier((L, 1.2), (82.0, 2.2), (66.0, 4.4), (56.0, 9.4), n=16)
    body += cubic_bezier((56.0, 9.4), (48.0, 11.4), (38.0, 10.4), (26.0, 5.2), n=16)
    body += cubic_bezier((26.0, 5.2), (15.0, 2.6), (6.0, 1.8), (0.0, 1.4), n=12)
    body.append((0.0, 0.4))
    body += cubic_bezier((0.0, 0.4), (24.0, -0.6), (62.0, -0.8), (L, 1.2), n=16)
    s.panel(body, fill=0, width=1.6)

    # Faceted stealth cockpit windscreen panes
    s.solid([(62.5, 6.6), (56.5, 9.4), (51.5, 9.6), (51.5, 7.2), (61.5, 6.2)])
    # Flush sawtooth dorsal engine intake & recessed stealth exhaust trough
    s.panel([(42.0, 8.8), (31.0, 8.2), (29.0, 5.8), (40.5, 6.2)], fill=0, width=1.25)
    s.solid([(41.8, 8.4), (39.5, 8.3), (38.5, 6.4), (40.5, 6.5)])
    # Leading-edge sweep crease along the flying wing
    s.stroke(cubic_bezier((92.0, 1.4), (66.0, 2.8), (38.0, 3.0), (10.0, 1.4), n=14), width=0.95)
    return s


def b_v22():
    s = Sil()
    L, h = 100.0, 12.8

    far_fin = [(14.2, h * 0.48), (6.5, h * 0.48), (1.2, h * 1.92), (7.6, h * 1.92)]
    s.panel(far_fin, fill=0, width=1.2)

    fus = []
    fus += cubic_bezier((L, h * 0.34), (L, h * 0.56), (L - 3.5, h * 0.68), (L - 8.5, h * 0.72), n=12)
    fus += cubic_bezier((L - 8.5, h * 0.72), (L - 11.5, h * 0.94), (L - 14.5, h * 1.04), (L - 19.0, h * 1.04), n=10)
    fus.append((12.5, h * 0.98))
    fus.append((5.5, h * 0.92))
    fus.append((5.5, h * 0.66))
    fus += cubic_bezier((5.5, h * 0.66), (17.0, h * 0.44), (27.5, h * 0.08), (37.5, 0.0), n=14)
    fus.append((L - 12.0, 0.0))
    fus += cubic_bezier((L - 12.0, 0.0), (L - 4.5, 0.02), (L, h * 0.10), (L, h * 0.34), n=12)
    s.panel(fus, fill=0, width=1.6)

    s.stroke([(L - 5.5, h * 0.68), (L - 5.5, h * 0.08)], width=0.9)
    s.panel(ellipse(L - 7.8, -0.3, 1.6, 1.2, n=12), fill=0, width=1.0, min_w=300)
    s.stroke([(33.5, h * 0.06), (26.5, h * 0.48), (15.0, h * 0.62)], width=0.9)

    s.solid([(L - 8.8, h * 0.56), (L - 11.4, h * 0.88), (L - 14.8, h * 0.88), (L - 14.8, h * 0.56)])
    s.solid([(L - 15.6, h * 0.56), (L - 15.6, h * 0.84), (L - 18.8, h * 0.84), (L - 18.8, h * 0.56)])

    draw_door(s, 73.5, h * 0.14, h * 0.78, w=4.2)
    for wx in (63.5, 55.5, 47.5, 39.5):
        s.solid(ellipse(wx, h * 0.56, 1.12, 1.12, n=12))

    near_fin = [(16.2, h * 0.44), (8.6, h * 0.44), (3.4, h * 1.96), (9.8, h * 1.96)]
    s.panel(near_fin, fill=0, width=1.5)
    s.stroke([(5.8, h * 1.96), (10.2, h * 0.46)], width=0.9)
    s.panel([(16.0, h * 0.78), (8.2, h * 0.78), (7.8, h * 0.64), (15.5, h * 0.64)], fill=0, width=1.15)

    sponson = (
        cubic_bezier((71.0, 0.2), (68.0, 4.0), (44.0, 4.0), (39.0, 0.8), n=14)
        + cubic_bezier((39.0, 0.8), (43.0, -1.6), (67.0, -1.6), (71.0, 0.2), n=14)
    )
    s.panel(sponson, fill=0, width=1.35)
    s.stroke([(58.0, 3.0), (58.0, -1.0)], width=0.85, min_w=300)

    s.panel(
        cubic_bezier((67.0, h * 1.02), (63.0, h * 1.30), (47.0, h * 1.30), (43.0, h * 1.02), n=12)
        + [(43.0, h * 0.92), (67.0, h * 0.92)],
        fill=0, width=1.3,
    )

    ang = math.radians(-12.0)
    ca, sa = math.cos(ang), math.sin(ang)
    cx, cy = 54.5, h * 1.12

    def rot(lx, ly):
        return (cx + lx * ca - ly * sa, cy + lx * sa + ly * ca)

    nac_local = (
        cubic_bezier((4.2, -2.2), (4.8, 3.0), (4.5, 11.2), (3.3, 15.2), n=10)
        + [(3.3, 15.2), (-3.3, 15.2)]
        + cubic_bezier((-3.3, 15.2), (-4.5, 11.2), (-4.8, 3.0), (-4.0, -2.2), n=10)
        + cubic_bezier((-4.0, -2.2), (-3.0, -4.2), (3.0, -4.2), (4.2, -2.2), n=8)
    )
    s.panel([rot(x, y) for x, y in nac_local], fill=0, width=1.5)
    s.stroke([rot(4.2, 6.2), rot(-4.2, 6.2)], width=0.9)
    s.panel([rot(2.4, -3.3), rot(1.9, -6.2), rot(-1.9, -6.2), rot(-2.4, -3.3)], fill=0, width=1.15)
    s.solid([rot(1.5, -5.3), rot(1.5, -6.2), rot(-1.5, -6.2), rot(-1.5, -5.3)])

    hub_local = (
        [rot(2.9, 15.2)]
        + [rot(2.9 * math.cos(a), 15.2 + 3.8 * math.sin(a))
           for a in (math.radians(i * 15) for i in range(13))]
    )
    s.panel(hub_local, fill=0, width=1.35)

    blade_r = 23.5
    blade_local = [
        (-blade_r, 16.7), (-3.8, 17.5), (3.8, 17.5), (blade_r, 16.7),
        (blade_r, 15.8), (3.8, 16.1), (-3.8, 16.1), (-blade_r, 15.8),
    ]
    s.panel([rot(x, y) for x, y in blade_local], fill=1, width=0.9)
    return s


def b_sleigh():
    """Christmas easter egg: Santa's sleigh behind a team of reindeer (technical line style)."""
    s = Sil()
    # Gift sack behind Santa
    s.panel(ellipse(8.0, 18.5, 6.5, 6.0, n=20), fill=0, width=1.4)
    # Santa torso, head, hat & pompom
    s.panel(ellipse(20.0, 16.5, 5.0, 6.0, n=18), fill=0, width=1.4)
    s.panel(ellipse(21.0, 25.0, 3.0, 3.0, n=16), fill=0, width=1.35)
    s.panel([(18.0, 27.2), (24.0, 27.2), (21.5, 33.2), (18.8, 32.4)], fill=0, width=1.2)
    s.panel(ellipse(18.4, 33.4, 1.3, 1.3, n=10), fill=0, width=1.0)
    s.solid(ellipse(22.4, 25.4, 0.7, 0.7, n=8))

    # Ornate sleigh body with curled front prow
    body = (
        [(0.0, 6.5), (1.5, 15.5), (7.5, 16.5), (10.5, 11.5), (29.5, 11.5)]
        + cubic_bezier((29.5, 11.5), (33.5, 17.5), (37.5, 19.0), (39.5, 15.5), n=10)
        + cubic_bezier((39.5, 15.5), (37.5, 10.0), (35.0, 4.5), (32.5, 3.5), n=8)
        + [(4.0, 3.5), (0.0, 6.5)]
    )
    s.panel(body, fill=0, width=1.55)
    s.stroke([(6.0, 7.2), (32.0, 7.2)], width=0.9, min_w=300)

    # Runner with graceful front curl + struts
    runner = [(-2.0, -1.0), (35.5, -1.0)] + quad_bezier((35.5, -1.0), (43.5, -1.0), (41.5, 6.0), 10)
    s.stroke(runner, width=1.45)
    s.stroke([(8.5, 3.5), (8.5, -1.0)], width=1.25)
    s.stroke([(29.5, 3.5), (29.5, -1.0)], width=1.25)

    # Team of 3 galloping reindeer + harness
    s.stroke([(37.5, 13.5), (52.0, 12.5), (72.0, 14.0), (92.0, 12.5)], width=0.9)
    for i, x in enumerate((52.0, 72.0, 92.0)):
        y = 12.0 + (i % 2) * 1.5
        s.stroke([(x + 5.0, y - 2.0), (x + 10.0, y - 6.0)], width=1.1)
        s.stroke([(x + 3.0, y - 2.0), (x + 7.0, y - 7.0)], width=1.1)
        s.stroke([(x - 5.0, y - 2.0), (x - 10.0, y - 5.0)], width=1.1)
        s.stroke([(x - 3.0, y - 2.0), (x - 7.0, y - 7.0)], width=1.1)
        s.panel(ellipse(x, y, 7.2, 3.4, n=20), fill=0, width=1.35)
        s.panel([(x + 4.8, y + 1.8), (x + 8.8, y + 8.8), (x + 11.0, y + 7.8), (x + 7.5, y - 0.2)],
                fill=0, width=1.25)
        s.panel(ellipse(x + 11.4, y + 9.2, 2.9, 1.6, n=14), fill=0, width=1.2)
        s.stroke([(x + 9.8, y + 10.8), (x + 8.8, y + 14.8), (x + 6.8, y + 16.8)], width=0.95)
        s.stroke([(x + 8.8, y + 14.8), (x + 11.0, y + 16.5)], width=0.95)
        s.stroke([(x + 10.8, y + 10.8), (x + 11.8, y + 14.8), (x + 13.8, y + 15.8)], width=0.95)
        s.panel(ellipse(x - 7.4, y + 1.8, 1.4, 0.9, n=8), fill=0, width=1.0)
    return s


BUILDERS = {name[2:]: fn for name, fn in globals().items() if name.startswith("b_") and callable(fn)}
