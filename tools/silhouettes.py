"""Parametric side-view aircraft silhouettes (nose to the right).

Every family in data/families.csv has a builder here. Shapes are described in
"design units" (fuselage length ~100, y pointing up), then fitted and rasterised
to 1-bit bitmaps by gen_assets.py. Near parts are drawn with a thin white
separation outline so overlapping shapes (engines over the fuselage, far vs
near engines) stay readable on a black-and-white panel.
"""
import math

# ----------------------------------------------------------------------------
# Primitive helpers (all return lists of (x, y) points, y up)


def ellipse(cx, cy, rx, ry, a0=0.0, a1=360.0, n=48):
    pts = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        pts.append((cx + rx * math.cos(a), cy + ry * math.sin(a)))
    return pts


def quad_bezier(p0, p1, p2, n=16):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])
            for t in (i / n for i in range(n + 1))]


def capsule(x0, x1, yc, h):
    """Horizontal capsule from x0 (rear) to x1 (front)."""
    r = h / 2
    return ellipse(x1 - r, yc, r, r, -90, 90, 16) + ellipse(x0 + r, yc, r, r, 90, 270, 16)


class Sil:
    def __init__(self):
        self.ops = []  # (kind, points, fill, sep)

    def poly(self, pts, fill=1, sep=0.0):
        self.ops.append(("poly", pts, fill, sep))
        return self

    def line(self, pts, width, fill=1):
        self.ops.append(("line", pts, fill, width))
        return self

    def white(self, pts):
        return self.poly(pts, fill=0)


# ----------------------------------------------------------------------------
# Composite parts


def fuselage(s, L=100.0, h=10.0, nose_len=7.0, nose="round", tip=0.42, tail_len=24.0,
             tail_top=0.9, tail_bot=0.62, crown=None, bottom_drop=0.0):
    """Main body. crown(x) may raise the top line (747 upper deck, cargo bulges)."""
    top = crown or (lambda x: h)
    hn = top(L - nose_len)  # height where the upper nose curve meets the crown
    pts = []
    if nose == "round":
        nose_up = ellipse(L - nose_len, tip * h, nose_len, hn - tip * h, 0, 90, 14)
        nose_lo = ellipse(L - nose_len, tip * h, nose_len, tip * h + bottom_drop, 270, 360, 14)
    elif nose == "pointed":
        nose_up = quad_bezier((L, tip * h), (L - nose_len * 0.45, hn * 0.98), (L - nose_len, hn))
        nose_lo = quad_bezier((L - nose_len, -bottom_drop), (L - nose_len * 0.4, 0), (L, tip * h))
    elif nose == "blunt":
        nose_up = ellipse(L - nose_len, tip * h, nose_len, hn - tip * h, 0, 90, 10)
        nose_lo = ellipse(L - nose_len, tip * h, nose_len * 0.8, tip * h + bottom_drop, 270, 360, 10)
    else:
        raise ValueError(nose)
    pts += nose_up
    steps = 60
    x_top_end = L - nose_len
    for i in range(steps + 1):
        x = x_top_end - (x_top_end - tail_len * 0.55) * i / steps
        pts.append((x, top(x)))
    pts += quad_bezier((tail_len * 0.55, h), (tail_len * 0.2, h), (0, tail_top * h), 10)
    pts.append((0, tail_bot * h))
    pts += quad_bezier((0, tail_bot * h), (tail_len * 0.7, 0), (tail_len * 1.1, -bottom_drop), 12)
    pts.append((L - nose_len, -bottom_drop))
    pts += nose_lo
    s.poly(pts)
    return s


def upper_deck(h, hh, rear_x, ramp=12.0):
    """Crown function: height hh forward of rear_x, easing down to h over `ramp`."""
    def crown(x):
        if x >= rear_x:
            return hh
        if x >= rear_x - ramp:
            f = (x - (rear_x - ramp)) / ramp
            return h + (hh - h) * (0.5 - 0.5 * math.cos(math.pi * f))
        return h
    return crown


def cockpit(s, L, h, nose_len, y0=0.6, y1=0.78, length=1.0):
    """White windscreen slit near the nose."""
    x1 = L - nose_len * 0.42
    x0 = L - nose_len * (0.42 + 0.75 * length)
    s.white([(x0, y0 * h), (x1 - nose_len * 0.1, y0 * h), (x1, y0 * h + (y1 - y0) * h * 0.45),
             (x1 - nose_len * 0.25, y1 * h), (x0 + nose_len * 0.05, y1 * h)])


def windows(s, x0, x1, y, size, spacing):
    x = x1
    while x > x0:
        s.white(ellipse(x, y, size * 0.5, size * 0.6, n=10))
        x -= spacing


def fin(s, x_front, x_rear, y_root, height, sweep, tip_chord, sep=0.0):
    s.poly([(x_rear, y_root), (x_front, y_root), (x_front - sweep, y_root + height),
            (x_front - sweep - tip_chord, y_root + height), (x_rear - sweep * 0.25, y_root + height * 0.25)],
           sep=sep)


def stab(s, x_front, x_rear, y, thick, sep=0.0):
    length = x_front - x_rear
    s.poly([(x_front, y), (x_front - length * 0.25, y + thick * 0.5), (x_rear, y + thick * 0.25),
            (x_rear, y - thick * 0.25), (x_front - length * 0.25, y - thick * 0.5)], sep=sep)


def nacelle(s, x_front, length, yc, h, sep=1.0, nozzle=0.65):
    """Turbofan pod: rounded intake on the right, tapered nozzle on the left."""
    r = h / 2
    pts = ellipse(x_front - r * 0.6, yc, r * 0.6, r, -90, 90, 14)
    pts += [(x_front - length * 0.7, yc + r), (x_front - length, yc + r * nozzle)]
    pts += [(x_front - length, yc - r * nozzle), (x_front - length * 0.7, yc - r)]
    s.poly(pts, sep=sep)


def pylon(s, x, y_top, y_bot, width):
    s.poly([(x, y_top), (x - width, y_top), (x - width * 1.25, y_bot), (x - width * 0.1, y_bot)])


def under_engine(s, x_front, length, h, fus_bottom=0.0, drop=0.25, sep=1.0):
    """Underwing pod. drop < 0 tucks the pod up so it overlaps the fuselage belly."""
    yc = fus_bottom - drop * h - h * 0.3
    nacelle(s, x_front, length, yc, h, sep=sep)
    if drop > -0.2:
        pylon(s, x_front - length * 0.3, fus_bottom + 0.3, yc + h * 0.45, length * 0.4)


def high_wing(s, x_front, chord, y, thick, sep=0.0):
    pts = ellipse(x_front - chord * 0.2, y, chord * 0.2, thick, 0, 180, 10)
    pts += [(x_front - chord, y + thick * 0.25), (x_front - chord, y - thick * 0.1), (x_front - chord * 0.2, y - thick * 0.3)]
    s.poly(pts, sep=sep)


def prop_disc(s, x, yc, radius, width=0.9):
    s.poly([(x - width / 2, yc - radius), (x + width / 2, yc - radius),
            (x + width / 2, yc + radius), (x - width / 2, yc + radius)], sep=0.5)
    s.poly(ellipse(x + width, yc, width * 1.6, width * 1.4, n=12))  # spinner


def turboprop(s, x_front, length, yc, h, prop_r, sep=1.0):
    pts = ellipse(x_front - h * 0.5, yc, h * 0.5, h / 2, -90, 90, 10)
    pts += [(x_front - length, yc + h * 0.25), (x_front - length, yc - h * 0.15)]
    s.poly(pts, sep=sep)
    prop_disc(s, x_front + 0.6, yc, prop_r)


def wheels(s, xs, y, r):
    for x in xs:
        s.poly(ellipse(x, y, r, r, n=14))
        s.poly([(x - r * 0.25, y), (x + r * 0.25, y), (x + r * 0.15, y + r * 2.4), (x - r * 0.15, y + r * 2.4)])


def rotor_line(s, xc, y, span, thick=0.9):
    s.poly([(xc - span / 2, y), (xc + span / 2, y), (xc + span / 2, y + thick), (xc - span / 2, y + thick)])


# ----------------------------------------------------------------------------
# Generic builders


def airliner(h=10.5, nose="round", nose_len=7.0, tip=0.42, fin_h=13.0, fin_root=(21.0, 1.0), fin_sweep=10.0,
             fin_tip=6.5, t_tail=False, engines="under2", eng_x=58.0, eng_len=12.0, eng_h=5.5, eng_drop=-0.1,
             hump=None, tail_top=0.9, tail_bot=0.6, tail_len=24.0, wins=True, stab_y=None,
             winglet=False, cockpit_len=1.0):
    s = Sil()
    L = 100.0
    fin_top = h * 0.8 + fin_h + h * 0.2
    if t_tail:
        tip_front = fin_root[0] - fin_sweep
        stab(s, tip_front + 2.5, tip_front - fin_tip - 7, fin_top - 0.8, 2.2)
    fin(s, fin_root[0], fin_root[1], h * 0.8, fin_h + h * 0.2, fin_sweep, fin_tip)
    if engines == "under4":
        under_engine(s, eng_x - 13, eng_len * 0.92, eng_h * 0.9, drop=eng_drop + 0.35, sep=0)
    if winglet:
        s.poly([(44, h + 0.5), (42, h + 0.5), (40, h + 4.5), (41.2, h + 4.5)])
    crown = None
    nl = nose_len
    if hump:
        rear_x, hh = hump
        crown = upper_deck(h, hh, rear_x)
        nl = nose_len * 1.6
    fuselage(s, L, h, nl, nose, tip, tail_len=tail_len, tail_top=tail_top, tail_bot=tail_bot, crown=crown)
    if not t_tail:
        sy = stab_y if stab_y is not None else h * 0.66
        stab(s, fin_root[0] - 2, -3.5, sy, 2.4)
    if wins:
        windows(s, tail_len * 0.95, L - nl - 5, h * 0.62, h * 0.11, 2.3)
    cockpit(s, L, h * (1.0 if not hump else 1.05), nose_len, length=cockpit_len)
    s.poly(ellipse(52, 0.6, 14, h * 0.18, 180, 360, 16))  # belly fairing
    if engines in ("under2", "under4"):
        under_engine(s, eng_x, eng_len, eng_h, drop=eng_drop)
    elif engines in ("rear2", "rear3"):
        nacelle(s, 33 if engines == "rear2" else 30, eng_len, h * 0.92, eng_h, sep=1.0)
        pylon(s, 26, h * 0.92, h * 0.6, 4)
        if engines == "rear3":
            s.poly(ellipse(fin_root[0] + 1, h * 1.02, 4, 2.0, 0, 180, 10))  # S-duct intake
    elif engines == "tail3":
        under_engine(s, eng_x, eng_len, eng_h, drop=eng_drop)
        nacelle(s, fin_root[0] + 4, eng_len * 1.05, h + eng_h * 0.55, eng_h * 0.95, sep=1.0)
    return s


def high_winger(h=11.0, nose="round", nose_len=7.0, tip=0.45, t_tail=True, fin_h=14.0, fin_sweep=11.0,
                engines="tp2", eng_x=62.0, eng_len=14.0, eng_h=4.5, prop_r=7.5, upsweep=True, sponson=True,
                wing_x=64.0, wing_chord=20.0, extra=None, tail_len=30.0, wins=False, glazed=False):
    s = Sil()
    L = 100.0
    if t_tail:
        stab(s, 24 - fin_sweep + 2.5, 24 - fin_sweep - 14, h + fin_h - 0.8, 2.4)
    fin(s, 24, 4, h * 0.8, fin_h + h * 0.2, fin_sweep, 7)
    if engines in ("tp4", "jet4"):
        if engines == "tp4":
            turboprop(s, eng_x - 14, eng_len * 0.9, h * 0.72, eng_h * 0.9, prop_r * 0.92, sep=0)
        else:
            nacelle(s, eng_x - 11, eng_len * 0.92, h * 0.25, eng_h * 0.92, sep=0)
            pylon(s, eng_x - 14, h + 0.5, h * 0.4, 5)
    fuselage(s, L, h, nose_len, nose, tip, tail_len=tail_len, tail_top=0.95, tail_bot=0.72 if upsweep else 0.6)
    if not t_tail:
        stab(s, 22, -3, h * 0.7, 2.6)
    high_wing(s, wing_x, wing_chord, h + 0.3, 2.4)
    if sponson:
        s.poly(ellipse(56, 0.4, 13, 2.6, 180, 360, 16))
    if wins:
        windows(s, 30, L - nose_len - 6, h * 0.6, h * 0.1, 3.0)
    cockpit(s, L, h, nose_len, y0=0.64, y1=0.8)
    if glazed:
        for i in range(3):
            s.white(ellipse(L - 2.2 - i * 1.6, h * 0.25, 0.55, 0.9, n=8))
    if engines == "tp2":
        turboprop(s, eng_x, eng_len, h * 0.72, eng_h, prop_r)
    elif engines == "tp4":
        turboprop(s, eng_x, eng_len, h * 0.62, eng_h, prop_r)
    elif engines in ("jet4", "jet2"):
        nacelle(s, eng_x, eng_len, h * 0.12, eng_h, sep=1.0)
        pylon(s, eng_x - eng_len * 0.25, h + 0.5, h * 0.4, eng_len * 0.4)
    if extra:
        extra(s, h)
    return s


# ----------------------------------------------------------------------------
# Family builders


def b_generic_jet():
    return airliner()


def b_a220():
    return airliner(h=10.0, eng_len=13.5, eng_h=6.5, fin_h=14, eng_x=60)


def b_a320():
    return airliner(h=10.5)


def b_a300():
    return airliner(h=12.0, eng_len=13, eng_h=6.2, fin_h=15)


def b_a330():
    return airliner(h=10.8, eng_len=12.5, eng_h=6.6, fin_h=15, fin_sweep=11, eng_x=60)


def b_a340():
    return airliner(h=9.8, engines="under4", eng_len=10, eng_h=4.6, fin_h=14.5, eng_x=62)


def b_a350():
    s = airliner(h=10.8, eng_len=13, eng_h=7.0, fin_h=15.5, eng_x=60, cockpit_len=1.3)
    return s


def b_a380():
    return airliner(h=16.5, nose_len=9, tip=0.36, fin_h=19, fin_sweep=12, fin_tip=8, engines="under4",
                    eng_len=11, eng_h=6.6, eng_x=62, fin_root=(24, 2))


def _beluga(scale_len=1.0, bulge_h=2.35):
    s = Sil()
    L = 100.0
    h = 11.0
    # Endplate fins on the tailplane
    s.poly([(10, h * 0.55), (4, h * 0.55), (2, h * 1.35), (6, h * 1.35)])
    fin(s, 22, 3, h * 0.8, 15 + h * 0.2, 10, 7)
    # Lower fuselage (A300 based), cockpit low at the nose
    fuselage(s, L, h, 6.5, "round", 0.45, tail_len=24, tail_top=0.9, tail_bot=0.6)
    # Giant upper bulge
    x_rear, x_front = 14.0, 92.0 * scale_len if scale_len < 1 else 92.0
    top = bulge_h * h
    pts = quad_bezier((x_rear, h * 0.7), (x_rear + 6, top * 0.98), (x_rear + 22, top))
    pts += [(x_front - 18, top)]
    pts += quad_bezier((x_front - 18, top), (x_front + 6, top * 0.98), (x_front - 4, h * 0.78))
    s.poly(pts)
    stab(s, 20, -2.5, h * 0.62, 2.4)
    cockpit(s, L, h, 6.5, y0=0.45, y1=0.65)
    under_engine(s, 60, 13, 6.2, drop=0.15)
    return s


def b_beluga():
    return _beluga(bulge_h=2.2)


def b_belugaxl():
    return _beluga(bulge_h=2.45)


def b_b717():
    return airliner(h=10.0, nose="pointed", nose_len=9, t_tail=True, engines="rear2", eng_len=11, eng_h=4.6,
                    fin_h=13, fin_sweep=13)


def b_b727():
    return airliner(h=10.0, nose="pointed", nose_len=9, t_tail=True, engines="rear3", eng_len=11, eng_h=4.4,
                    fin_h=15, fin_sweep=14)


def b_b737():
    return airliner(h=10.0, nose="pointed", nose_len=9, tip=0.38, eng_x=61, eng_len=10.5, eng_h=5.2, eng_drop=0.0)


def b_b737max():
    return airliner(h=10.0, nose="pointed", nose_len=9, tip=0.38, eng_x=63, eng_len=12.5, eng_h=6.0, eng_drop=0.05,
                    winglet=True)


def b_b747():
    return airliner(h=12.0, nose_len=9, tip=0.32, fin_h=18, fin_sweep=12, engines="under4", eng_len=11.5,
                    eng_h=5.6, eng_x=62, hump=(64, 12.0 * 1.4))


def b_dreamlifter():
    s = b_b747()
    h = 12.0
    top = h * 1.95
    pts = quad_bezier((18, h * 0.8), (24, top), (40, top)) + [(64, top)]
    pts += quad_bezier((64, top), (76, top), (82, h * 1.3))
    s.poly(pts)
    return s


def b_b757():
    return airliner(h=9.2, nose="pointed", nose_len=10, tip=0.36, fin_h=14.5, eng_len=12.5, eng_h=6.0, eng_x=62)


def b_b767():
    return airliner(h=11.2, nose="pointed", nose_len=9, tip=0.38, fin_h=15, eng_len=12.5, eng_h=6.2, eng_x=60)


def b_b777():
    return airliner(h=11.2, nose_len=8, fin_h=15.5, eng_len=14.5, eng_h=8.0, eng_x=61, tail_top=0.95, tail_bot=0.45)


def b_b787():
    return airliner(h=11.0, nose_len=9, tip=0.4, fin_h=15.5, fin_sweep=12, eng_len=13.5, eng_h=7.2, eng_x=60,
                    cockpit_len=0.9)


def b_md80():
    return airliner(h=8.8, nose="pointed", nose_len=10, t_tail=True, engines="rear2", eng_len=12, eng_h=4.4,
                    fin_h=13, fin_sweep=14, tail_len=20)


def b_dc10():
    return airliner(h=11.5, nose_len=8, engines="tail3", eng_len=12.5, eng_h=6.0, eng_x=60, fin_h=14,
                    fin_root=(18, 1), fin_sweep=11)


def b_ejet():
    return airliner(h=10.5, nose="pointed", nose_len=8, fin_h=14, eng_len=11.5, eng_h=5.8, eng_x=59)


def b_erj():
    return airliner(h=8.0, nose="pointed", nose_len=11, t_tail=True, engines="rear2", eng_len=11, eng_h=4.6,
                    fin_h=12, fin_sweep=12)


def b_crj():
    return airliner(h=8.3, nose="pointed", nose_len=10, t_tail=True, engines="rear2", eng_len=12, eng_h=5.2,
                    fin_h=13, fin_sweep=13)


def b_atr():
    return high_winger(h=10.5, wins=True, sponson=True, upsweep=False, eng_x=63, prop_r=7.0, tail_len=26)


def b_dash8():
    return high_winger(h=10.0, nose="pointed", nose_len=9, wins=True, upsweep=False, eng_x=66, eng_len=24,
                       prop_r=7.5, tail_len=26, fin_h=16)


def b_saab():
    s = airliner(h=10.5, nose="pointed", nose_len=9, engines="none", fin_h=13, fin_sweep=9, eng_x=0)
    turboprop(s, 64, 16, -1.2, 4.6, 7.0)
    return s


def b_f100():
    return airliner(h=9.0, nose="pointed", nose_len=9, t_tail=True, engines="rear2", eng_len=12, eng_h=5.0,
                    fin_h=13, fin_sweep=12)


def b_bae146():
    return high_winger(h=10.5, nose="round", nose_len=7, engines="jet4", eng_x=63, eng_len=10, eng_h=4.6,
                       fin_h=15, fin_sweep=12, upsweep=False, sponson=True, wins=True, tail_len=26)


def b_comac():
    return airliner(h=10.6, nose="round", nose_len=8, eng_len=12.5, eng_h=6.0)


def b_superjet():
    return airliner(h=10.2, nose="pointed", nose_len=8, eng_len=12, eng_h=6.0, eng_x=61)


def b_il76():
    return high_winger(h=11.5, nose="round", nose_len=7, tip=0.55, engines="jet4", eng_x=63, eng_len=12,
                       eng_h=4.8, fin_h=15, glazed=True)


def b_an124():
    return high_winger(h=13.0, nose="round", nose_len=8, tip=0.6, t_tail=False, engines="jet4", eng_x=63,
                       eng_len=13, eng_h=6.4, fin_h=17, fin_sweep=12)


def b_an225():
    def endplates(s, h):
        s.poly([(10, h * 0.7), (3, h * 0.7), (1, h * 1.75), (6, h * 1.75)], sep=0.6)
    s = high_winger(h=12.0, nose="round", nose_len=8, tip=0.6, t_tail=False, engines="jet4", eng_x=66,
                    eng_len=12, eng_h=6.0, fin_h=2, extra=endplates)
    nacelle(s, 55, 11, 12 * 0.12 - 1, 5.4, sep=1.0)
    return s


def b_superguppy():
    s = Sil()
    h = 10.0
    fin(s, 22, 2, h * 0.8, 18, 6, 9)
    fuselage(s, 100, h, 7, "round", 0.4, tail_len=22, tail_top=0.9, tail_bot=0.6)
    top = h * 2.6
    s.poly(quad_bezier((12, h * 0.8), (16, top), (36, top)) + [(70, top)] +
           quad_bezier((70, top), (98, top * 0.95), (96, h * 0.6)))
    stab(s, 20, -2, h * 0.6, 2.4)
    cockpit(s, 100, h, 7, y0=0.4, y1=0.6)
    turboprop(s, 58, 14, -0.5, 4.4, 7.5, sep=0)
    turboprop(s, 66, 14, -1.5, 4.6, 8.0)
    return s


def b_concorde():
    s = Sil()
    h = 6.0
    fin(s, 26, 3, h * 0.8, 17, 14, 4)
    fuselage(s, 100, h, 16, "pointed", 0.15, tail_len=18, tail_top=0.85, tail_bot=0.55)
    # Delta wing seen edge-on with its engine boxes
    s.poly([(62, 0.5), (8, 0.5), (6, -1.6), (40, -1.6)])
    s.poly([(44, -0.8), (16, -0.8), (16, -4.6), (40, -4.6)], sep=0.8)
    s.white([(97.5, 0.65 * h), (93, 0.65 * h), (92, 0.85 * h), (95, 0.85 * h)])
    windows(s, 26, 88, h * 0.62, h * 0.12, 2.2)
    return s


def b_lightjet():
    s = airliner(h=13.5, nose="pointed", nose_len=14, tip=0.38, t_tail=False, engines="rear2", eng_len=16,
                 eng_h=7.0, fin_h=18, fin_sweep=14, fin_root=(24, 2), wins=False, tail_len=34, stab_y=13.5 * 1.25)
    windows(s, 42, 78, 13.5 * 0.6, 13.5 * 0.16, 5.5)
    return s


def b_bizjet():
    s = airliner(h=11.0, nose="pointed", nose_len=13, tip=0.38, t_tail=True, engines="rear2", eng_len=15,
                 eng_h=6.2, fin_h=16, fin_sweep=15, fin_root=(24, 2), wins=False, tail_len=30, winglet=True)
    windows(s, 40, 80, 11 * 0.6, 11 * 0.2, 5.0)
    return s


def _light(high=True, gear=True):
    s = Sil()
    L, h = 100.0, 20.0
    fin(s, 24, 2, h * 0.75, 22, 9, 10)
    # Fuselage: cowling at the front, cabin, slim tailcone
    pts = ellipse(92, h * 0.5, 6, h * 0.42, -90, 90, 12)
    pts += [(78, h * 0.95), (56, h * 1.1 if not high else h * 0.98), (40, h * 0.9), (6, h * 0.82), (0, h * 0.76),
            (0, h * 0.6), (40, h * 0.12), (70, 0), (86, h * 0.06)]
    s.poly(pts)
    stab(s, 22, -2, h * 0.7, 3.2)
    s.white([(74, h * 0.85), (62, h * 0.85), (60, h * 0.6), (72, h * 0.6)])  # side window
    s.white([(78, h * 0.86), (76, h * 0.86), (74.5, h * 0.62), (79, h * 0.66)])  # windscreen
    prop_disc(s, 98.5, h * 0.5, 15, width=1.4)
    if high:
        high_wing(s, 76, 26, h * 1.0, 2.6)
        s.line([(64, h * 0.98), (58, h * 0.1)], 1.4)  # strut
    else:
        s.poly([(80, h * 0.05), (52, h * 0.1), (50, -1.2), (78, -1.6)], sep=0.8)
        s.poly(ellipse(66, h * 0.98, 13, h * 0.32, 0, 180, 14))  # canopy/cabin roof
        s.white([(76, h * 1.05), (60, h * 1.05), (58, h * 0.88), (78, h * 0.88)])
    if gear:
        wheels(s, [82, 60], -6, 3.2)
    return s


def b_single_high():
    return _light(True)


def b_single_low():
    return _light(False)


def b_twin_piston():
    s = Sil()
    h = 15.0
    fin(s, 24, 2, h * 0.8, 18, 10, 9)
    fuselage(s, 100, h, 16, "pointed", 0.4, tail_len=34, tail_top=0.9, tail_bot=0.6)
    stab(s, 22, -2, h * 0.65, 3.0)
    s.white([(84, h * 0.84), (80, h * 0.84), (78, h * 0.6), (88, h * 0.6)])
    windows(s, 50, 74, h * 0.62, h * 0.18, 7)
    turboprop(s, 78, 22, -0.5, 6.0, 11.5)
    return s


def b_turboprop_single():
    s = airliner(h=14.0, nose="pointed", nose_len=18, tip=0.45, t_tail=True, engines="none", fin_h=17, fin_sweep=13,
                 fin_root=(24, 2), wins=False, tail_len=34)
    windows(s, 42, 72, 14 * 0.62, 14 * 0.18, 6)
    prop_disc(s, 100.5, 14 * 0.45, 13, width=1.3)
    s.poly([(70, 0.5), (46, 0.5), (44, -1.4), (68, -1.8)], sep=0.8)
    return s


def b_twin_turboprop():
    s = airliner(h=13.5, nose="pointed", nose_len=15, tip=0.4, t_tail=True, engines="none", fin_h=17, fin_sweep=14,
                 fin_root=(24, 2), wins=False, tail_len=32)
    windows(s, 40, 72, 13.5 * 0.62, 13.5 * 0.2, 6)
    turboprop(s, 74, 22, -0.8, 6.0, 11)
    return s


def _heli(rotor_span=96.0, body_len=46.0, h=22.0, skids=True, tail="fin"):
    s = Sil()
    # Tail boom
    s.poly([(52, h * 0.65), (6, h * 0.62), (6, h * 0.5), (52, h * 0.35)])
    if tail == "fin":
        s.poly([(10, h * 0.5), (2, h * 0.55), (0, h * 1.3), (5, h * 1.3)])
        s.poly(ellipse(6, h * 0.95, 5.0, 5.0, n=20), fill=1, sep=0.8)
        s.white(ellipse(6, h * 0.95, 3.6, 3.6, n=20))
    stab(s, 22, 12, h * 0.52, 2.2)
    # Cabin pod
    pts = ellipse(100 - body_len * 0.45, h * 0.5, body_len * 0.45, h * 0.5, -90, 90, 20)
    pts += [(46, h * 0.95), (42, h * 0.6), (46, h * 0.1), (100 - body_len * 0.45, 0)]
    s.poly(pts)
    s.poly([(70, h), (52, h), (54, h * 1.2), (68, h * 1.2)])  # engine cowling
    s.white(ellipse(86, h * 0.62, 9, h * 0.26, -60, 110, 14) + [(80, h * 0.4)])  # glazing
    s.white([(74, h * 0.8), (64, h * 0.8), (64, h * 0.45), (74, h * 0.45)])  # door window
    s.poly([(62, h * 1.2), (60, h * 1.2), (60, h * 1.45), (62, h * 1.45)])  # mast
    rotor_line(s, 61, h * 1.45, rotor_span, 1.4)
    if skids:
        s.line([(80, 0), (82, -6)], 1.2)
        s.line([(58, 0), (56, -6)], 1.2)
        s.poly([(92, -5.2), (88, -6.6), (46, -6.6), (46, -5.4)])
    return s


def b_helicopter():
    return _heli()


def b_chinook():
    s = Sil()
    h = 16.0
    s.poly([(100, h * 0.55), (97, h), (14, h), (8, h * 1.6), (0, h * 1.6), (0, h * 0.4), (10, 0), (94, 0)])
    s.poly([(96, h), (90, h * 1.25), (84, h)])  # front pylon
    s.poly([(26, h * 1.6), (18, h * 1.95), (8, h * 1.95), (4, h * 1.6)])  # rear pylon
    rotor_line(s, 88, h * 1.3, 70, 1.3)
    rotor_line(s, 13, h * 2.0, 70, 1.3)
    s.white(ellipse(94, h * 0.68, 4.5, h * 0.18, -60, 120, 10) + [(91, h * 0.5)])
    windows(s, 30, 80, h * 0.6, h * 0.16, 6)
    s.poly(capsule(14, 30, h * 0.75, 6.0), sep=0.8)  # engine pod
    wheels(s, [84, 28], -5, 3.0)
    return s


def b_glider():
    s = Sil()
    h = 9.0
    s.poly([(12, h * 0.6), (4, h * 0.6), (2, h * 2.4), (8, h * 2.4)])
    stab(s, 12, -2, h * 2.4, 1.6)
    pts = ellipse(86, h * 0.45, 14, h * 0.5, -90, 90, 16)
    pts += [(60, h * 0.9), (6, h * 0.6), (6, h * 0.35), (60, 0)]
    s.poly(pts)
    s.poly(ellipse(84, h * 0.8, 10, h * 0.32, 0, 180, 14))
    s.white(ellipse(84, h * 0.8, 8.5, h * 0.22, 5, 175, 14))
    s.poly([(72, h * 0.95), (64, h * 0.95), (63, h * 1.25), (70, h * 1.25)], sep=0.6)
    return s


def b_balloon():
    s = Sil()
    env = ellipse(50, 62, 30, 32, -40, 220, 40) + [(40, 20), (60, 20)]
    s.poly(env)
    for x0, x1 in ((40, 45), (60, 55), (50, 50)):
        s.line([(x0, 20), (x1, 10)], 0.9)
    s.poly([(55, 10), (45, 10), (46, 2), (54, 2)])
    for dx in (-15, 0, 15):
        s.white([(50 + dx * 0.95 - 1, 92), (50 + dx * 0.95 + 1, 92), (50 + dx * 0.6 + 1, 26), (50 + dx * 0.6 - 1, 26)])
    return s


def b_uav():
    s = Sil()
    h = 8.0
    s.poly([(14, h * 0.7), (4, h * 0.7), (0, h * 2.2), (4, h * 2.2)])  # V-tail
    pts = ellipse(80, h * 0.75, 20, h * 0.75, -90, 90, 18)
    pts += [(60, h * 1.5), (20, h * 1.0), (2, h * 0.75), (2, h * 0.35), (30, 0), (80, 0)]
    s.poly(pts)
    s.poly(capsule(6, 40, h * 1.35, 5.5), sep=0.8)  # dorsal engine
    s.poly([(62, h * 0.4), (40, h * 0.4), (38, h * 0.0), (60, -0.4)], sep=0.6)
    return s


def b_fighter():
    s = Sil()
    h = 9.0
    fin(s, 30, 6, h * 0.85, 15, 13, 5)
    pts = quad_bezier((100, h * 0.45), (92, h * 0.85), (80, h * 0.95))
    pts += [(56, h * 1.05), (30, h * 0.95), (4, h * 0.8), (0, h * 0.75), (0, h * 0.25), (6, h * 0.15),
            (40, 0), (60, -2.5), (74, -2.6), (78, 0.5)]
    pts += quad_bezier((80, h * 0.2), (92, h * 0.2), (100, h * 0.45))
    s.poly(pts)
    # Canopy
    s.poly(ellipse(72, h * 0.95, 11, h * 0.5, 0, 180, 18))
    s.white(ellipse(72, h * 0.95, 9.4, h * 0.36, 8, 172, 18))
    stab(s, 18, -2, h * 0.45, 3.0, sep=0.8)
    s.poly([(60, h * 0.25), (26, h * 0.25), (22, -0.6), (52, -0.6)], sep=0.6)  # wing edge-on
    s.poly([(4, h * 0.75), (-4, h * 0.7), (-4, h * 0.3), (4, h * 0.25)])  # nozzle
    return s


def b_jet_trainer():
    s = Sil()
    h = 11.0
    fin(s, 28, 4, h * 0.85, 16, 12, 6)
    pts = quad_bezier((100, h * 0.4), (94, h * 0.8), (82, h * 0.9))
    pts += [(40, h * 1.0), (4, h * 0.8), (0, h * 0.7), (0, h * 0.3), (30, 0), (84, h * 0.05)]
    pts += quad_bezier((84, h * 0.05), (94, h * 0.1), (100, h * 0.4))
    s.poly(pts)
    s.poly(ellipse(64, h * 0.95, 18, h * 0.55, 0, 180, 20))
    s.white(ellipse(64, h * 0.95, 16, h * 0.42, 6, 174, 20))
    s.poly([(64, h * 1.5), (63, h * 1.5), (63, h * 0.95), (64, h * 0.95)])  # canopy frame
    stab(s, 18, -2, h * 0.55, 3.0, sep=0.8)
    s.poly(capsule(46, 62, h * 0.55, h * 0.5), sep=0.8)  # intake
    return s


def b_prop_trainer():
    s = Sil()
    h = 12.0
    fin(s, 26, 3, h * 0.8, 17, 10, 7)
    pts = ellipse(92, h * 0.5, 7, h * 0.45, -90, 90, 12)
    pts += [(70, h * 0.98), (30, h * 0.9), (2, h * 0.8), (0, h * 0.6), (30, h * 0.1), (70, 0), (86, h * 0.06)]
    s.poly(pts)
    s.poly(ellipse(62, h * 0.95, 20, h * 0.5, 0, 180, 20))
    s.white(ellipse(62, h * 0.95, 18, h * 0.38, 6, 174, 20))
    stab(s, 20, -2, h * 0.65, 3.0, sep=0.8)
    s.poly([(74, h * 0.05), (50, h * 0.1), (48, -1.2), (72, -1.6)], sep=0.8)
    prop_disc(s, 99.5, h * 0.5, 14, width=1.3)
    return s


def b_a400m():
    return high_winger(h=13.0, nose="round", nose_len=7, tip=0.45, engines="tp4", eng_x=64, eng_len=14, eng_h=5.2,
                       prop_r=9.5, fin_h=17, fin_sweep=10)


def b_c130():
    return high_winger(h=12.5, nose="round", nose_len=8, tip=0.48, t_tail=False, engines="tp4", eng_x=62,
                       eng_len=13, eng_h=5.0, prop_r=8.5, fin_h=18, fin_sweep=8)


def b_c17():
    return high_winger(h=14.0, nose="round", nose_len=8, tip=0.4, engines="jet4", eng_x=62, eng_len=13, eng_h=6.6,
                       fin_h=17, fin_sweep=13)


def b_c5():
    return high_winger(h=12.0, nose="round", nose_len=9, tip=0.62, engines="jet4", eng_x=63, eng_len=12, eng_h=6.0,
                       fin_h=16, fin_sweep=13)


def b_kc135():
    s = airliner(h=10.5, nose="round", nose_len=8, engines="under4", eng_len=10, eng_h=4.2, eng_x=62, fin_h=16,
                 fin_sweep=12)
    s.line([(16, 2.5), (4, -9)], 1.3)  # refuelling boom
    return s


def b_awacs():
    s = b_kc135()
    h = 10.5
    s.poly([(36, h), (34, h), (30, h + 4.5), (32, h + 4.5)])
    s.poly([(30, h), (28, h), (26, h + 4.5), (28, h + 4.5)])
    s.poly(ellipse(30, h + 6.2, 17, 2.6, n=30), sep=0.8)
    return s


def b_mil_twinprop():
    return high_winger(h=11.5, nose="round", nose_len=7, tip=0.45, t_tail=False, engines="tp2", eng_x=64,
                       eng_len=15, eng_h=5.2, prop_r=8.5, fin_h=17, fin_sweep=11)


def b_b52():
    s = Sil()
    h = 8.0
    s.poly([(26, h * 0.8), (6, h * 0.8), (8, h + 22), (16, h + 22)])  # tall fin
    fuselage(s, 100, h, 6, "round", 0.5, tail_len=10, tail_top=1.0, tail_bot=0.4)
    stab(s, 14, -2, h * 0.75, 2.4)
    high_wing(s, 66, 28, h + 0.2, 1.6)
    cockpit(s, 100, h, 6, y0=0.62, y1=0.85)
    for xf, yc, sep in ((52, -1.0, 0), (60, -3.0, 1.0)):
        pylon(s, xf - 5, h, yc, 3)
        s.poly(capsule(xf - 14, xf, yc, 4.4), sep=sep)
        s.poly([(xf - 7, yc + 2.2), (xf - 7.5, yc + 2.2), (xf - 7.5, yc - 2.2), (xf - 7, yc - 2.2)], fill=0)
    return s


def b_b1():
    s = Sil()
    h = 8.0
    fin(s, 26, 4, h * 0.8, 17, 14, 6)
    stab(s, 22, 4, h + 8, 2.2)
    pts = quad_bezier((100, h * 0.3), (90, h * 0.9), (74, h * 1.05))
    pts += [(40, h * 1.0), (4, h * 0.85), (0, h * 0.6), (10, h * 0.1), (60, 0)]
    pts += quad_bezier((60, 0), (88, 0), (100, h * 0.3))
    s.poly(pts)
    s.white([(88, h * 0.82), (84, h * 0.82), (83, h * 0.62), (90, h * 0.62)])
    s.poly([(44, 1), (12, 1), (8, -5), (36, -5)], sep=1.0)  # engine nacelles
    s.poly([(70, h * 0.4), (34, h * 0.4), (30, h * 0.15), (66, h * 0.15)], sep=0.6)  # swept wing
    return s


def b_b2():
    s = Sil()
    pts = quad_bezier((100, 1.0), (70, 3.0), (52, 8.5))
    pts += quad_bezier((52, 8.5), (40, 9.5), (30, 4.5))
    pts += [(0, 1.6), (0, 0.6), (40, -0.8), (80, -0.4)]
    s.poly(pts)
    s.white([(60, 7.8), (54, 8.4), (54, 7.0), (62, 6.6)])
    s.poly([(36, 6.2), (26, 6.4), (24, 4.6), (34, 4.4)], sep=0.5)  # intake
    return s


def b_v22():
    s = Sil()
    h = 12.0
    s.poly([(16, h * 0.8), (6, h * 0.8), (4, h * 1.9), (10, h * 1.9)])
    fuselage(s, 100, h, 9, "round", 0.42, tail_len=34, tail_top=0.95, tail_bot=0.7)
    stab(s, 18, -2, h * 0.95, 2.4)
    high_wing(s, 62, 20, h + 0.3, 1.8)
    # Vertical engine nacelle at the wing tip, rotor on top
    s.poly([(56, h - 1), (48, h - 1), (48, h + 16), (56, h + 16)], sep=1.0)
    rotor_line(s, 52, h + 17, 64, 1.4)
    cockpit(s, 100, h, 9, y0=0.6, y1=0.82)
    s.poly(ellipse(56, 0.4, 14, 2.8, 180, 360, 16))
    return s


BUILDERS = {name[2:]: fn for name, fn in globals().items() if name.startswith("b_") and callable(fn)}
