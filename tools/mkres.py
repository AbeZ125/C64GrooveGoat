#!/usr/bin/env python3
#
# C64 GrooveGoat resource generator
#
# Produces src/gres.c with:
#  - two bitmap font atlases (8x16 and 16x32 pixels per cell) rendered from
#    Fira Code (SIL Open Font License) plus procedurally drawn UI glyphs
#  - the logo as RGBA images in several sizes
#  - embedded text resources (6502 playroutines, PRG frontend, manual)
#
import os, sys, math
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTDIR = os.environ.get('GG_FONTDIR', os.path.join(ROOT, 'res', 'fonts'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import mklogo

SCALES = [(8, 16, 'FiraCode-SemiBold.ttf', 12.5), (16, 32, 'FiraCode-Medium.ttf', 25.0)]

# The fonts that can be chosen on the screen (the Aa button): name, and for the 8x16 and 16x32 cells the font file,
# the size and the vertical stretch. The pixel fonts (8 pixels per character) are drawn at exactly 8 and 16 pixels and
# stretched 2x vertically to fill the tall cells without blurring. The first font is the default.
# Fonts without the Latin-1 letters (codes 160-255): those characters are taken from the first font
NOLATIN1 = ()

FONTS = [
    ('Fira Code', [(8, 16, 'FiraCode-SemiBold.ttf', 12.5, 1), (16, 32, 'FiraCode-Medium.ttf', 25.0, 1)]),
    ('MesloLGS NF', [(8, 16, 'MesloLGS-NF-Regular.ttf', 12.5, 1), (16, 32, 'MesloLGS-NF-Regular.ttf', 25.0, 1)]),
]

# Custom glyph codes (keep in sync with gglyphs.h)
G = {
    'HLINE': 1, 'VLINE': 2, 'TL': 3, 'TR': 4, 'BL': 5, 'BR': 6, 'LT': 7, 'RT': 8,
    'TT': 9, 'BT': 10, 'CROSS': 11, 'FULL': 12, 'UPPER': 13, 'LOWER': 14,
    'LEFTH': 15, 'RIGHTH': 16, 'SHADE1': 17, 'SHADE2': 18, 'PLAY': 19, 'PLAYL': 20,
    'UP': 21, 'DOWN': 22, 'STOP': 23, 'DOT': 24, 'NOTE': 25, 'SUN': 26, 'MOON': 27,
    'CHECK': 28, 'BULLET': 29, 'PAUSE': 30, 'MENU': 31,
    'SLIDERON': 128, 'SLIDEROFF': 129, 'SLIDERKNOB': 130, 'RBR': 131, 'BTNL': 132, 'BTNR': 133,
    'FOLDER': 134, 'DISK': 135, 'CHIP': 136, 'INFO': 137, 'HELP': 138, 'PLAYPOS': 139,
    'PLAYPAT': 140, 'NEWDOC': 141, 'LOADINS': 142, 'SAVEINS': 143, 'BAR1': 144,
    'BAR2': 145, 'BAR3': 146, 'BAR4': 147, 'BAR5': 148, 'BAR6': 149, 'BAR7': 150,
    'BAR8': 151, 'SPEAKER': 152, 'MUTE': 153, 'SOLO': 154, 'PREV': 155, 'NEXT': 156,
    'PRG': 157, 'GEAR': 158, 'UNDO': 159,
    # rarely used Latin-1 symbols replaced by UI glyphs
    'WAVE': 164, 'FILM': 165, 'REDO': 166, 'STAR': 168, 'STAROFF': 172, 'DICE': 173, 'DICER': 174,
    'SEARCH': 175, 'ENV': 127,
}

def glyph_custom(code, cw, ch):
    """Return an 'L' image cw x ch for custom glyph code, or None."""
    SS = 4
    W, H = cw * SS, ch * SS
    im = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(im)
    s = cw / 8.0  # stroke scale
    lw = max(1, int(round(s))) * SS
    cx, cy = W // 2, H // 2
    name = None
    for k, v in G.items():
        if v == code:
            name = k
    if name is None:
        return None

    if name in ('DICE', 'DICER'):
        # Big dice drawn over two cells: DICE is the left, DICER the right half
        big = Image.new('L', (W * 2, H), 0)
        bd = ImageDraw.Draw(big)
        side = H * 0.74
        x0, y0 = W - side / 2, cy - side / 2
        bd.rounded_rectangle([x0, y0, x0 + side, y0 + side], radius=side * 0.2, fill=255)
        r = side * 0.095
        for (px, py) in ((-0.25, -0.25), (0.25, -0.25), (0.0, 0.0), (-0.25, 0.25), (0.25, 0.25)):
            x, y = W + side * px, cy + side * py
            bd.ellipse([x - r, y - r, x + r, y + r], fill=0)
        half = big.crop((0, 0, W, H) if name == 'DICE' else (W, 0, W * 2, H))
        return half.resize((cw, ch), Image.LANCZOS)

    def hl(x0, x1):
        d.rectangle([x0, cy - lw // 2, x1, cy - lw // 2 + lw - 1], fill=255)

    def vl(y0, y1):
        d.rectangle([cx - lw // 2, y0, cx - lw // 2 + lw - 1, y1], fill=255)

    if name == 'HLINE': hl(0, W)
    elif name == 'VLINE': vl(0, H)
    elif name == 'TL': hl(cx - lw // 2, W); vl(cy - lw // 2, H)
    elif name == 'TR': hl(0, cx + lw // 2); vl(cy - lw // 2, H)
    elif name == 'BL': hl(cx - lw // 2, W); vl(0, cy + lw // 2)
    elif name == 'BR': hl(0, cx + lw // 2); vl(0, cy + lw // 2)
    elif name == 'LT': vl(0, H); hl(cx, W)
    elif name == 'RT': vl(0, H); hl(0, cx)
    elif name == 'TT': hl(0, W); vl(cy, H)
    elif name == 'BT': hl(0, W); vl(0, cy)
    elif name == 'CROSS': hl(0, W); vl(0, H)
    elif name == 'FULL': d.rectangle([0, 0, W, H], fill=255)
    elif name == 'UPPER': d.rectangle([0, 0, W, H // 2 - 1], fill=255)
    elif name == 'LOWER': d.rectangle([0, H // 2, W, H], fill=255)
    elif name == 'LEFTH': d.rectangle([0, 0, W // 2 - 1, H], fill=255)
    elif name == 'RIGHTH': d.rectangle([W // 2, 0, W, H], fill=255)
    elif name in ('SHADE1', 'SHADE2'):
        step = 2 * SS * max(1, int(s))
        for y in range(0, H, step):
            for x in range(0, W, step):
                if name == 'SHADE1' and ((x // step + y // step) % 2): continue
                d.rectangle([x, y, x + step // 2 if name == 'SHADE1' else x + step - 1, y + step // 2], fill=255)
    elif name in ('PLAY', 'PLAYL', 'UP', 'DOWN'):
        m = W * 0.18
        t = H * 0.30
        if name == 'PLAY': pts = [(m, t), (W - m, cy), (m, H - t)]
        elif name == 'PLAYL': pts = [(W - m, t), (m, cy), (W - m, H - t)]
        elif name == 'UP': pts = [(m, cy + W * 0.25), (cx, cy - W * 0.25), (W - m, cy + W * 0.25)]
        else: pts = [(m, cy - W * 0.25), (cx, cy + W * 0.25), (W - m, cy - W * 0.25)]
        d.polygon(pts, fill=255)
    elif name == 'STOP':
        m = W * 0.2
        d.rectangle([m, cy - (W / 2 - m), W - m, cy + (W / 2 - m)], fill=255)
    elif name == 'PAUSE':
        m = W * 0.18
        d.rectangle([m, cy - W * 0.3, m + W * 0.2, cy + W * 0.3], fill=255)
        d.rectangle([W - m - W * 0.2, cy - W * 0.3, W - m, cy + W * 0.3], fill=255)
    elif name == 'DOT':
        r = W * 0.30
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    elif name == 'BULLET':
        r = W * 0.16
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    elif name == 'NOTE':
        r = W * 0.22
        hx, hy = cx - W * 0.08, cy + H * 0.18
        d.ellipse([hx - r * 1.25, hy - r, hx + r * 1.25, hy + r], fill=255)
        sx = hx + r * 1.0
        d.rectangle([sx - lw, cy - H * 0.30, sx, hy], fill=255)
        d.polygon([(sx, cy - H * 0.30), (sx + W * 0.35, cy - H * 0.12), (sx + W * 0.3, cy - H * 0.04), (sx, cy - H * 0.18)], fill=255)
    elif name == 'SUN':
        r = W * 0.20
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        for k in range(8):
            a = k * math.pi / 4
            x0, y0 = cx + math.cos(a) * W * 0.30, cy + math.sin(a) * W * 0.30
            x1, y1 = cx + math.cos(a) * W * 0.44, cy + math.sin(a) * W * 0.44
            d.line([x0, y0, x1, y1], fill=255, width=max(SS, lw))
    elif name == 'MOON':
        r = W * 0.38
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        o = W * 0.22
        d.ellipse([cx - r + o, cy - r - o * 0.5, cx + r + o, cy + r - o * 0.5], fill=0)
    elif name == 'CHECK':
        d.line([W * 0.15, cy, W * 0.40, cy + W * 0.25, W * 0.85, cy - W * 0.30], fill=255, width=max(SS, lw * 2 // 1))
    elif name == 'MENU':
        for k in (-1, 0, 1):
            y = cy + k * W * 0.28
            d.rectangle([W * 0.15, y - lw // 2, W * 0.85, y + lw // 2], fill=255)
    elif name == 'SLIDERON':
        # filled part of a slider: band in the middle of the cell
        d.rectangle([0, H * 0.36, W, H * 0.64], fill=255)
    elif name == 'SLIDEROFF':
        d.rectangle([0, H * 0.44, W, H * 0.56], fill=255)
    elif name == 'SLIDERKNOB':
        d.rounded_rectangle([W * 0.12, H * 0.20, W * 0.88, H * 0.80], radius=W * 0.3, fill=255)
    elif name in ('RTL', 'RTR', 'RBL', 'RBR'):
        # rounded corners for panels: quarter circle arcs joining lines at the centre
        r = min(W, H) // 2
        box = {'RTL': [cx, cy, cx + 2 * r, cy + 2 * r], 'RTR': [cx - 2 * r, cy, cx, cy + 2 * r],
               'RBL': [cx, cy - 2 * r, cx + 2 * r, cy], 'RBR': [cx - 2 * r, cy - 2 * r, cx, cy]}[name]
        start = {'RTL': 180, 'RTR': 270, 'RBL': 90, 'RBR': 0}[name]
        d.arc(box, start, start + 90, fill=255, width=lw)
        if name in ('RTL', 'RBL'): hl(cx + r, W)
        else: hl(0, cx - r)
        if name in ('RTL', 'RTR'): vl(cy + r, H)
        else: vl(0, cy - r)
    elif name in ('BTNL', 'BTNR'):
        # half-round end caps of buttons (filled)
        if name == 'BTNL':
            d.ellipse([0, H * 0.12, W * 2, H * 0.88], fill=255)
        else:
            d.ellipse([-W, H * 0.12, W, H * 0.88], fill=255)
    elif name == 'FOLDER':
        d.rectangle([W * 0.08, H * 0.36, W * 0.92, H * 0.74], fill=255)
        d.rectangle([W * 0.08, H * 0.30, W * 0.45, H * 0.38], fill=255)
    elif name == 'DISK':
        # floppy disk: body with a cut corner, shutter notch at the top and
        # a label window that does not reach the bottom edge
        d.polygon([(W * 0.06, H * 0.28), (W * 0.74, H * 0.28), (W * 0.94, H * 0.38),
                   (W * 0.94, H * 0.76), (W * 0.06, H * 0.76)], fill=255)
        d.rectangle([W * 0.26, H * 0.28, W * 0.62, H * 0.40], fill=0)
        d.rectangle([W * 0.24, H * 0.53, W * 0.76, H * 0.69], fill=0)
    elif name == 'CHIP':
        d.rectangle([W * 0.22, H * 0.28, W * 0.78, H * 0.72], fill=255)
        for k in range(4):
            y = H * (0.31 + k * 0.12)
            d.rectangle([W * 0.05, y, W * 0.22, y + H * 0.04], fill=255)
            d.rectangle([W * 0.78, y, W * 0.95, y + H * 0.04], fill=255)
    elif name == 'INFO':
        r = W * 0.42
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        d.rectangle([cx - lw // 2 - SS // 2, cy - r * 0.1, cx + lw // 2 + SS // 2, cy + r * 0.62], fill=0)
        d.ellipse([cx - lw, cy - r * 0.55 - lw, cx + lw, cy - r * 0.55 + lw], fill=0)
    elif name == 'HELP':
        r = W * 0.42
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        f = ImageFont.truetype(os.path.join(FONTDIR, 'FiraCode-Bold.ttf'), int(H * 0.42))
        tw = f.getlength('?')
        asc, desc = f.getmetrics()
        d.text((cx - tw / 2, cy - (asc + desc) / 2 - H * 0.01), '?', font=f, fill=0)
    elif name == 'PLAYPOS':
        m = W * 0.18
        d.polygon([(m, H * 0.30), (W * 0.70, cy), (m, H * 0.70)], fill=255)
        d.rectangle([W * 0.72, H * 0.30, W * 0.86, H * 0.70], fill=255)
    elif name == 'PLAYPAT':
        d.rectangle([W * 0.08, H * 0.28, W * 0.92, H * 0.72], fill=255)
        m = W * 0.30
        d.polygon([(m, H * 0.36), (W * 0.74, cy), (m, H * 0.64)], fill=0)
    elif name == 'NEWDOC':
        d.polygon([(W * 0.2, H * 0.25), (W * 0.6, H * 0.25), (W * 0.8, H * 0.37), (W * 0.8, H * 0.75), (W * 0.2, H * 0.75)], fill=255)
        d.polygon([(W * 0.6, H * 0.25), (W * 0.6, H * 0.37), (W * 0.8, H * 0.37)], fill=0)
    elif name in ('LOADINS', 'SAVEINS'):
        d.rectangle([W * 0.12, H * 0.62, W * 0.88, H * 0.74], fill=255)
        if name == 'LOADINS':
            d.polygon([(W * 0.2, H * 0.40), (W * 0.8, H * 0.40), (cx, H * 0.58)], fill=255)
            d.rectangle([cx - lw, H * 0.24, cx + lw, H * 0.42], fill=255)
        else:
            d.polygon([(W * 0.2, H * 0.42), (W * 0.8, H * 0.42), (cx, H * 0.24)], fill=255)
            d.rectangle([cx - lw, H * 0.40, cx + lw, H * 0.58], fill=255)
    elif name.startswith('BAR'):
        n = int(name[3:])
        d.rectangle([0, H - H * n / 8.0, W, H], fill=255)
    elif name in ('SPEAKER', 'MUTE', 'SOLO'):
        d.polygon([(W * 0.10, H * 0.42), (W * 0.30, H * 0.42), (W * 0.55, H * 0.28), (W * 0.55, H * 0.72), (W * 0.30, H * 0.58), (W * 0.10, H * 0.58)], fill=255)
        if name == 'SPEAKER':
            d.arc([W * 0.45, H * 0.32, W * 0.85, H * 0.68], 300, 60, fill=255, width=lw)
        elif name == 'MUTE':
            d.line([W * 0.62, H * 0.38, W * 0.92, H * 0.62], fill=255, width=lw)
            d.line([W * 0.62, H * 0.62, W * 0.92, H * 0.38], fill=255, width=lw)
        else:
            d.ellipse([W * 0.66, H * 0.44, W * 0.86, H * 0.56], fill=255)
    elif name in ('PREV', 'NEXT'):
        if name == 'PREV':
            d.polygon([(W * 0.75, H * 0.32), (W * 0.30, cy), (W * 0.75, H * 0.68)], fill=255)
        else:
            d.polygon([(W * 0.25, H * 0.32), (W * 0.70, cy), (W * 0.25, H * 0.68)], fill=255)
    elif name == 'PRG':
        # tiny breadbin computer
        d.polygon([(W * 0.08, H * 0.62), (W * 0.18, H * 0.38), (W * 0.82, H * 0.38), (W * 0.92, H * 0.62)], fill=255)
        for k in range(3):
            y = H * (0.44 + k * 0.05)
            d.rectangle([W * 0.26, y, W * 0.74, y + H * 0.02], fill=0)
    elif name == 'GEAR':
        r = W * 0.30
        for k in range(8):
            a = k * math.pi / 4
            x, y = cx + math.cos(a) * r, cy + math.sin(a) * r
            d.ellipse([x - W * 0.1, y - W * 0.1, x + W * 0.1, y + W * 0.1], fill=255)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        d.ellipse([cx - r * 0.4, cy - r * 0.4, cx + r * 0.4, cy + r * 0.4], fill=0)
    elif name == 'UNDO':
        d.arc([W * 0.2, H * 0.32, W * 0.85, H * 0.72], 180, 90 + 360, fill=255, width=lw)
        d.polygon([(W * 0.05, H * 0.50), (W * 0.35, H * 0.50), (W * 0.2, H * 0.36)], fill=255)
    elif name == 'REDO':
        d.arc([W * 0.15, H * 0.32, W * 0.8, H * 0.72], 90, 360, fill=255, width=lw)
        d.polygon([(W * 0.95, H * 0.50), (W * 0.65, H * 0.50), (W * 0.8, H * 0.36)], fill=255)
    elif name == 'WAVE':
        # sound wave
        pts = []
        for k in range(41):
            t = k / 40.0
            pts.append((W * (0.06 + 0.88 * t), cy - H * 0.17 * math.sin(t * math.pi * 3) * (1 - abs(t - 0.5))))
        d.line(pts, fill=255, width=lw, joint='curve')
    elif name == 'FILM':
        d.rectangle([W * 0.08, H * 0.32, W * 0.92, H * 0.68], fill=255)
        for k in range(4):
            x = W * (0.14 + k * 0.2)
            d.rectangle([x, H * 0.35, x + W * 0.1, H * 0.39], fill=0)
            d.rectangle([x, H * 0.61, x + W * 0.1, H * 0.65], fill=0)
        d.rectangle([W * 0.14, H * 0.43, W * 0.86, H * 0.57], fill=0)
    elif name in ('STAR', 'STAROFF'):
        pts = []
        for k in range(10):
            a = -math.pi / 2 + k * math.pi / 5
            r = W * (0.44 if k % 2 == 0 else 0.18)
            pts.append((cx + math.cos(a) * r, cy + 0.02 * H + math.sin(a) * r))
        if name == 'STAR':
            d.polygon(pts, fill=255)
        else:
            d.polygon(pts, outline=255, width=max(SS, lw * 2 // 3))
    elif name == 'SEARCH':
        r = W * 0.26
        ox, oy = cx - W * 0.08, cy - W * 0.08
        d.ellipse([ox - r, oy - r, ox + r, oy + r], outline=255, width=lw)
        d.line([ox + r * 0.7, oy + r * 0.7, W * 0.92, cy + W * 0.4], fill=255, width=lw * 2)
    elif name == 'ENV':
        # ADSR envelope shape
        d.line([(W * 0.05, H * 0.7), (W * 0.3, H * 0.3), (W * 0.48, H * 0.5), (W * 0.72, H * 0.5), (W * 0.95, H * 0.7)], fill=255, width=lw)
    else:
        return None
    return im.resize((cw, ch), Image.LANCZOS)


def render_font(cw, ch, fontfile, size, vs=1):
    f = ImageFont.truetype(os.path.join(FONTDIR, fontfile), size)
    chd = ch // vs        # height of the glyph before the vertical stretch
    # Vertical placement: centre the ink of letters, digits, brackets and
    # descenders in the cell, so that nothing touches the top or the bottom
    # of the cell (the screen may be scaled with linear filtering)
    sample = 'ABCHbdfhklgjpqy0123456789()[]{}|$@'
    inktop = min(f.getbbox(c, anchor='ls')[1] for c in sample)
    inkbot = max(f.getbbox(c, anchor='ls')[3] for c in sample)
    baseline = int(round((chd - (inkbot - inktop)) / 2.0 - inktop))
    glyphs = []
    for code in range(256):
        g = None
        if code in G.values():
            g = glyph_custom(code, cw, ch)
        elif (32 <= code < 127) or (160 <= code < 256):
            c = chr(code)
            SS = 1
            g = Image.new('L', (cw, chd), 0)
            d = ImageDraw.Draw(g)
            adv = f.getlength(c)
            x = (cw - adv) / 2.0
            d.text((x, baseline), c, font=f, fill=255, anchor='ls')
            if vs != 1: g = g.resize((cw, ch), Image.NEAREST)
        if g is None:
            g = Image.new('L', (cw, ch), 0)
        glyphs.append(bytes(g.tobytes()))
    return glyphs


def cbytes(data, perline=24):
    out = []
    for i in range(0, len(data), perline):
        out.append(','.join(str(b) for b in data[i:i + perline]))
    return ',\n'.join(out)


def main():
    out = os.path.join(ROOT, 'src', 'gres.c')
    lines = ['// Generated by tools/mkres.py - do not edit', '#include <string.h>', '#include "gres.h"', '']
    for fi, (fname, scales) in enumerate(FONTS):
        for idx, (cw, ch, ff, sz, vs) in enumerate(scales):
            glyphs = render_font(cw, ch, ff, sz, vs)
            if fname in NOLATIN1:
                fira = render_font(*SCALES[idx][:2], SCALES[idx][2], SCALES[idx][3])
                for code in range(160, 256):
                    if code not in G.values(): glyphs[code] = fira[code]
            data = b''.join(glyphs)
            # the first font keeps the names res_font1 / res_font2 (the video export uses them)
            name = 'res_font%d' % (idx + 1) if fi == 0 else 'res_font%d_%d' % (fi, idx + 1)
            lines.append('const unsigned char %s[%d] = {' % (name, len(data)))
            lines.append(cbytes(data))
            lines.append('};')
    lines.append('const unsigned char *const res_fontset[%d][2] = {' % len(FONTS))
    for fi in range(len(FONTS)):
        n1 = 'res_font1' if fi == 0 else 'res_font%d_1' % fi
        n2 = 'res_font2' if fi == 0 else 'res_font%d_2' % fi
        lines.append('  {%s, %s},' % (n1, n2))
    lines.append('};')
    lines.append('const char *const res_fontnames[%d] = {%s};' % (len(FONTS), ', '.join('"%s"' % f[0] for f in FONTS)))
    # logo images
    big = mklogo.logo(1024)
    imgs = []
    for sz in (16, 32, 48, 64, 96, 128, 192, 256):
        im = big.resize((sz, sz), Image.LANCZOS)
        name = 'logo%d' % sz
        data = im.tobytes()
        lines.append('static const unsigned char res_%s[%d] = {' % (name, len(data)))
        lines.append(cbytes(data))
        lines.append('};')
        imgs.append((name, sz, sz))
    lines.append('const RESIMAGE res_images[] = {')
    for name, w, h in imgs:
        lines.append('  {"%s", %d, %d, res_%s},' % (name, w, h, name))
    lines.append('  {NULL, 0, 0, NULL}};')
    # embedded files
    files = [
        ('player.s', 'src/player.s'),
        ('altplayer.s', 'src/altplayer.s'),
        ('mplayer.s', 'src/mplayer.s'),
        ('frontend.s', 'src/frontend.s'),
        ('manual.txt', 'docs/manual.txt'),
        ('history.txt', 'docs/history.txt'),
        ('credits.txt', 'docs/credits.txt'),
    ]
    flist = []
    for i, (name, path) in enumerate(files):
        p = os.path.join(ROOT, path)
        if not os.path.exists(p):
            print('warning: missing', p)
            continue
        data = open(p, 'rb').read()
        if name.endswith('.txt'):
            # the fonts use Latin-1 (e.g. "Hülsbeck", "Öörni")
            data = data.decode('utf-8').encode('latin-1', 'replace')
        lines.append('static const unsigned char res_file%d[%d] = {' % (i, len(data) + 1))
        lines.append(cbytes(data + b'\0'))
        lines.append('};')
        flist.append((name, i, len(data)))
    lines.append('const RESFILE res_files[] = {')
    for name, i, n in flist:
        lines.append('  {"%s", res_file%d, %d},' % (name, i, n))
    lines.append('  {NULL, NULL, 0}};')
    lines.append('''
const unsigned char *res_getfile(const char *name, int *len)
{
  const RESFILE *f = res_files;
  while (f->name)
  {
    if (!strcmp(f->name, name))
    {
      if (len) *len = f->len;
      return f->data;
    }
    f++;
  }
  if (len) *len = 0;
  return NULL;
}

const RESIMAGE *res_getimage(const char *name)
{
  const RESIMAGE *i = res_images;
  while (i->name)
  {
    if (!strcmp(i->name, name)) return i;
    i++;
  }
  return NULL;
}
''')
    open(out, 'w').write('\n'.join(lines) + '\n')
    # header with glyph codes
    h = ['// Generated by tools/mkres.py - do not edit', '#ifndef GRES_H', '#define GRES_H', '',
         'typedef struct { const char *name; int w, h; const unsigned char *rgba; } RESIMAGE;',
         'typedef struct { const char *name; const unsigned char *data; int len; } RESFILE;', '',
         'extern const unsigned char res_font1[];', 'extern const unsigned char res_font2[];',
         '#define RES_NUMFONTS %d' % len(FONTS),
         'extern const unsigned char *const res_fontset[RES_NUMFONTS][2];',
         'extern const char *const res_fontnames[RES_NUMFONTS];',
         'extern const RESIMAGE res_images[];', 'extern const RESFILE res_files[];',
         'const unsigned char *res_getfile(const char *name, int *len);',
         'const RESIMAGE *res_getimage(const char *name);', '']
    for k, v in sorted(G.items(), key=lambda kv: kv[1]):
        h.append('#define GL_%s %d' % (k, v))
        h.append('#define GS_%s "\\x%02x"' % (k, v))
    h += ['', '#endif', '']
    open(os.path.join(ROOT, 'src', 'gres.h'), 'w').write('\n'.join(h))
    # preview image of glyphs
    if os.environ.get('GG_PREVIEW'):
        for fi, (fname, scales) in enumerate(FONTS):
            for idx, (cw, ch, ff, sz, vs) in enumerate(scales):
                glyphs = render_font(cw, ch, ff, sz, vs)
                if fname in NOLATIN1:
                    fira = render_font(*SCALES[idx][:2], SCALES[idx][2], SCALES[idx][3])
                    for code in range(160, 256):
                        if code not in G.values(): glyphs[code] = fira[code]
                pv = Image.new('L', (16 * (cw + 1), 16 * (ch + 1)), 40)
                for c in range(256):
                    g = Image.frombytes('L', (cw, ch), glyphs[c])
                    pv.paste(g, ((c % 16) * (cw + 1), (c // 16) * (ch + 1)))
                pv.save(os.environ['GG_PREVIEW'] + '%d_%d.png' % (fi, idx + 1))

if __name__ == '__main__':
    main()
