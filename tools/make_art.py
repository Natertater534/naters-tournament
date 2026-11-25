"""Draws the mod's effect sprites (all original art) in GD's three texture qualities.

Run: python3 tools/make_art.py  ->  resources/fx/<name>{,-hd,-uhd}.png
Sizes are in points; -uhd is 4x, -hd 2x, low 1x.
"""
import math, os
from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), '..', 'resources', 'fx')
os.makedirs(OUT, exist_ok=True)
S = 4  # uhd scale

INK = (16, 20, 40, 255)

def save(name, img):
    img.save(f'{OUT}/{name}-uhd.png', optimize=True)
    w, h = img.size
    img.resize((max(1, w // 2), max(1, h // 2)), Image.LANCZOS).save(f'{OUT}/{name}-hd.png', optimize=True)
    img.resize((max(1, w // 4), max(1, h // 4)), Image.LANCZOS).save(f'{OUT}/{name}.png', optimize=True)

def canvas(wpt, hpt):
    return Image.new('RGBA', (wpt * S, hpt * S), (0, 0, 0, 0))

def radial(wpt, hpt, stops):
    """stops: list of (t, alpha) from centre (0) to edge (1); white."""
    img = canvas(wpt, hpt)
    w, h = img.size
    px = img.load()
    for y in range(h):
        for x in range(w):
            dx = (x + .5 - w / 2) / (w / 2); dy = (y + .5 - h / 2) / (h / 2)
            t = math.sqrt(dx * dx + dy * dy)
            a = 0
            for (t0, a0), (t1, a1) in zip(stops, stops[1:]):
                if t0 <= t <= t1:
                    f = (t - t0) / (t1 - t0) if t1 > t0 else 0
                    a = a0 + (a1 - a0) * f
                    break
            px[x, y] = (255, 255, 255, int(max(0, min(255, a))))
    return img

# soft smoke puff: a cluster of overlapping blobs
def puff():
    img = canvas(32, 32); d = ImageDraw.Draw(img)
    c = 16 * S
    for (ox, oy, r) in [(0, 0, 9), (-7, -2, 6.5), (7, -1, 6.5), (-3, -7, 6), (4, -6.5, 6), (-5, 5.5, 6), (5, 5.5, 6), (0, 7, 5.5)]:
        d.ellipse([c + (ox - r) * S, c + (oy - r) * S, c + (ox + r) * S, c + (oy + r) * S], fill=(255, 255, 255, 255))
    img = img.filter(ImageFilter.GaussianBlur(1.6 * S))
    # gentle inner shading so it reads as a puff, not a disc
    shade = canvas(32, 32); sd = ImageDraw.Draw(shade)
    sd.ellipse([c - 8 * S, c - 1 * S, c + 9 * S, c + 12 * S], fill=(200, 210, 235, 90))
    shade = shade.filter(ImageFilter.GaussianBlur(3 * S))
    img = Image.alpha_composite(img, Image.composite(shade, Image.new('RGBA', img.size, (0, 0, 0, 0)), img.split()[3]))
    return img

def star():
    img = canvas(24, 24); d = ImageDraw.Draw(img); c = 12 * S
    pts = []
    for i in range(8):
        ang = math.pi / 4 * i - math.pi / 2
        r = (11 if i % 2 == 0 else 2.6) * S
        pts.append((c + math.cos(ang) * r, c + math.sin(ang) * r))
    glow = canvas(24, 24); ImageDraw.Draw(glow).ellipse([c - 6 * S, c - 6 * S, c + 6 * S, c + 6 * S], fill=(255, 255, 255, 130))
    glow = glow.filter(ImageFilter.GaussianBlur(2.5 * S))
    d.polygon(pts, fill=(255, 255, 255, 255))
    return Image.alpha_composite(glow, img)

def spark():
    img = canvas(32, 8); d = ImageDraw.Draw(img)
    w, h = img.size
    d.polygon([(0, h / 2), (w * .75, h * .08), (w, h / 2), (w * .75, h * .92)], fill=(255, 255, 255, 255))
    return img.filter(ImageFilter.GaussianBlur(.5 * S))

def ring():
    img = canvas(64, 64); d = ImageDraw.Draw(img); c = 32 * S
    d.ellipse([c - 29 * S, c - 29 * S, c + 29 * S, c + 29 * S], outline=(255, 255, 255, 255), width=4 * S)
    return img.filter(ImageFilter.GaussianBlur(.8 * S))

def dot():
    img = canvas(8, 8); ImageDraw.Draw(img).ellipse([S * .5, S * .5, 7.5 * S, 7.5 * S], fill=(255, 255, 255, 255))
    return img

def square():
    img = canvas(6, 6); ImageDraw.Draw(img).rectangle([0, 0, 6 * S - 1, 6 * S - 1], fill=(255, 255, 255, 255))
    return img

def glow():
    return radial(64, 64, [(0, 255), (.25, 170), (.6, 50), (1, 0)])

def shadow():
    img = radial(32, 10, [(0, 150), (.6, 90), (1, 0)])
    r, g, b, a = img.split()
    return Image.merge('RGBA', (r.point(lambda v: 0), g.point(lambda v: 0), b.point(lambda v: 0), a))

def rays():
    img = canvas(128, 128); d = ImageDraw.Draw(img); c = 64 * S
    for i in range(12):
        a0 = math.pi * 2 * i / 12; a1 = a0 + math.pi * 2 / 12 * .45
        d.polygon([(c, c), (c + math.cos(a0) * 64 * S, c + math.sin(a0) * 64 * S),
                   (c + math.cos(a1) * 64 * S, c + math.sin(a1) * 64 * S)], fill=(255, 255, 255, 255))
    img = img.filter(ImageFilter.GaussianBlur(1.5 * S))
    fade = radial(128, 128, [(0, 255), (.5, 160), (1, 0)])
    img.putalpha(Image.fromarray(__import__('numpy').minimum(__import__('numpy').array(img.split()[3]), __import__('numpy').array(fade.split()[3]))))
    return img

def bubble():
    # speech bubble, white with GD-thick dark outline, tail bottom-centre
    img = canvas(44, 40); d = ImageDraw.Draw(img)
    w, h = img.size; o = 2.2 * S
    body = [o, o, w - o, h - 9 * S]
    tail = [(w / 2 - 6 * S, h - 10 * S), (w / 2 + 6 * S, h - 10 * S), (w / 2, h - o)]
    d.rounded_rectangle([body[0] - 2 * S, body[1] - 2 * S, body[2] + 2 * S, body[3] + 2 * S], radius=12 * S, fill=INK)
    d.polygon([(tail[0][0] - 3 * S, tail[0][1]), (tail[1][0] + 3 * S, tail[1][1]), (tail[2][0], tail[2][1] + 1.5 * S)], fill=INK)
    d.rounded_rectangle(body, radius=10 * S, fill=(255, 255, 255, 255))
    d.polygon(tail, fill=(255, 255, 255, 255))
    d.rounded_rectangle([body[0] + 3 * S, body[1] + 2 * S, body[2] - 10 * S, body[1] + 6 * S], radius=2 * S, fill=(235, 242, 255, 255))
    return img

def trophy():
    img = canvas(48, 48); d = ImageDraw.Draw(img)
    def P(x, y): return (x * S, y * S)
    gold, light, dark = (255, 200, 60, 255), (255, 238, 150, 255), (196, 120, 20, 255)
    # handles
    for sx in (1, -1):
        cx = 24 + sx * 14
        d.ellipse([P(cx - 7, 9), P(cx + 7, 24)], outline=INK, width=int(4.5 * S))
        d.ellipse([P(cx - 7, 9), P(cx + 7, 24)], outline=gold, width=int(2 * S))
    # cup
    cup = [P(11, 6), P(37, 6), P(35, 18), P(30, 27), P(18, 27), P(13, 18)]
    d.polygon(cup, fill=INK)
    inner = [P(13.5, 8), P(34.5, 8), P(33, 18), P(29, 25), P(19, 25), P(15, 18)]
    d.polygon(inner, fill=gold)
    d.polygon([P(16, 9.5), P(21, 9.5), P(20, 20), P(17.5, 17)], fill=light)
    d.polygon([P(30, 9.5), P(33, 9.5), P(31, 19), P(28, 23)], fill=dark)
    # stem + base
    d.rectangle([P(20.5, 26), P(27.5, 34)], fill=INK); d.rectangle([P(22.5, 26), P(25.5, 34)], fill=dark)
    d.rounded_rectangle([P(13, 33), P(35, 42)], radius=2 * S, fill=INK)
    d.rounded_rectangle([P(15, 35), P(33, 40)], radius=1.5 * S, fill=gold)
    d.rectangle([P(16, 35.5), P(23, 37)], fill=light)
    # star
    c = (24 * S, 15.5 * S); pts = []
    for i in range(10):
        a = math.pi / 5 * i - math.pi / 2; r = (4.6 if i % 2 == 0 else 2) * S
        pts.append((c[0] + math.cos(a) * r, c[1] + math.sin(a) * r))
    d.polygon(pts, fill=(255, 255, 255, 255))
    return img

def crown():
    img = canvas(32, 24); d = ImageDraw.Draw(img)
    def P(x, y): return (x * S, y * S)
    gold, light, dark = (255, 205, 60, 255), (255, 240, 160, 255), (200, 125, 20, 255)
    outer = [P(2, 21), P(3, 6), P(9.5, 12), P(16, 2), P(22.5, 12), P(29, 6), P(30, 21)]
    d.polygon(outer, fill=INK)
    inner = [P(4.5, 19.5), P(5, 9.5), P(9.8, 14.5), P(16, 5.5), P(22.2, 14.5), P(27, 9.5), P(27.5, 19.5)]
    d.polygon(inner, fill=gold)
    d.polygon([P(5, 16), P(27.5, 16), P(27.5, 19.5), P(4.5, 19.5)], fill=dark)
    d.polygon([P(6, 10.5), P(9.5, 14), P(8, 15)], fill=light)
    for (x, col) in ((9.5, (90, 200, 255, 255)), (16, (255, 90, 110, 255)), (22.5, (120, 255, 150, 255))):
        d.ellipse([P(x - 1.8, 15.9), P(x + 1.8, 19.5)], fill=col, outline=INK, width=int(.7 * S))
    return img

def check():
    img = canvas(16, 16); d = ImageDraw.Draw(img)
    pts = [(3 * S, 8.5 * S), (6.5 * S, 12 * S), (13 * S, 4 * S)]
    d.line(pts, fill=INK, width=int(4.6 * S), joint='curve')
    d.line(pts, fill=(110, 255, 140, 255), width=int(2.4 * S), joint='curve')
    return img

def cross():
    img = canvas(16, 16); d = ImageDraw.Draw(img)
    for a, b in (((4, 4), (12, 12)), ((12, 4), (4, 12))):
        d.line([(a[0] * S, a[1] * S), (b[0] * S, b[1] * S)], fill=INK, width=int(4.6 * S))
    for a, b in (((4, 4), (12, 12)), ((12, 4), (4, 12))):
        d.line([(a[0] * S, a[1] * S), (b[0] * S, b[1] * S)], fill=(255, 80, 80, 255), width=int(2.4 * S))
    return img

def vignette():
    img = radial(128, 72, [(0, 0), (.55, 0), (1.0, 170), (1.5, 230)])
    r, g, b, a = img.split()
    return Image.merge('RGBA', (r.point(lambda v: 4), g.point(lambda v: 6), b.point(lambda v: 18), a))


def sword_img(scale=1.0):
    """One sword pointing up, handle at the bottom (16 x 52 pt). Glossy, thick dark outline
    like Nater's Timer Battles emotes."""
    W, H = 16, 52
    img = canvas(W, H); d = ImageDraw.Draw(img)
    def P(x, y): return (x * S, y * S)
    ink = INK
    steel, steelHi, steelLo = (214, 226, 245, 255), (255, 255, 255, 255), (138, 156, 190, 255)
    gold, goldHi, goldLo = (255, 196, 54, 255), (255, 236, 150, 255), (198, 120, 18, 255)
    grip, gripLo = (124, 70, 36, 255), (84, 44, 20, 255)
    # blade (outline, then fill, then a bevel and a shine)
    d.polygon([P(4.2, 33.5), P(4.2, 7.5), P(8, 1.2), P(11.8, 7.5), P(11.8, 33.5)], fill=ink)
    d.polygon([P(5.6, 33), P(5.6, 8), P(8, 3.6), P(10.4, 8), P(10.4, 33)], fill=steel)
    d.polygon([P(8, 4.4), P(10.4, 8.2), P(10.4, 33), P(8, 33)], fill=steelLo)
    d.polygon([P(6.4, 9), P(7.4, 7), P(7.4, 31.5), P(6.4, 31.5)], fill=steelHi)
    # fuller (the groove down the middle)
    d.rectangle([P(7.6, 10), P(8.4, 30)], fill=(112, 128, 168, 255))
    # crossguard
    d.rounded_rectangle([P(0.6, 32.2), P(15.4, 37.4)], radius=2.2 * S, fill=ink)
    d.rounded_rectangle([P(1.9, 33.3), P(14.1, 36.3)], radius=1.4 * S, fill=gold)
    d.rectangle([P(2.6, 33.6), P(13.4, 34.5)], fill=goldHi)
    d.rectangle([P(2.6, 35.4), P(13.4, 36.1)], fill=goldLo)
    d.ellipse([P(6.4, 33.2), P(9.6, 36.4)], fill=(90, 170, 255, 255), outline=ink, width=int(.7 * S))
    # grip with wraps
    d.rectangle([P(5.9, 37), P(10.1, 46.6)], fill=ink)
    d.rectangle([P(7, 37.3), P(9, 46.3)], fill=grip)
    for y in (39, 41.4, 43.8):
        d.line([P(7, y), P(9, y + 1.1)], fill=gripLo, width=int(.8 * S))
    # pommel
    d.ellipse([P(4.6, 45.2), P(11.4, 51.6)], fill=ink)
    d.ellipse([P(5.9, 46.4), P(10.1, 50.4)], fill=gold)
    d.ellipse([P(6.6, 46.9), P(8.2, 48.3)], fill=goldHi)
    return img

def sword():
    return sword_img()

def swords():
    """crossed swords, crossing a third of the way up the blades"""
    one = sword_img()
    w, h = one.size
    side = int(h * 1.15)
    pivot = (w / 2, 22 * S)  # a point on the blade, in sword-image pixels
    out = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    for ang in (40, -40):
        pad = Image.new('RGBA', (side, side), (0, 0, 0, 0))
        pad.alpha_composite(one, (int(side / 2 - pivot[0]), int(side / 2 - pivot[1])))
        out.alpha_composite(pad.rotate(ang, resample=Image.BICUBIC, center=(side / 2, side / 2)))
    bbox = out.getbbox()
    return out.crop(bbox)

def slash():
    """a crescent swoosh for sword swings: thick in the middle, thin at the tips"""
    img = canvas(48, 48); d = ImageDraw.Draw(img)
    cx, cy = 24 * S, 30 * S
    outer, steps = 21 * S, 40
    pts_out, pts_in = [], []
    for i in range(steps + 1):
        t = i / steps
        a = math.radians(200 + 140 * t)
        thick = (0.2 + 5.8 * math.sin(math.pi * t)) * S
        pts_out.append((cx + math.cos(a) * outer, cy + math.sin(a) * outer))
        pts_in.append((cx + math.cos(a) * (outer - thick), cy + math.sin(a) * (outer - thick)))
    d.polygon(pts_out + pts_in[::-1], fill=(255, 255, 255, 255))
    return img.filter(ImageFilter.GaussianBlur(.5 * S))

def badge():
    """the mod's own main-menu button: navy coin, gold rim, crossed swords"""
    img = canvas(56, 56); d = ImageDraw.Draw(img)
    c = 28 * S
    d.ellipse([c - 27 * S, c - 27 * S, c + 27 * S, c + 27 * S], fill=INK)
    d.ellipse([c - 25 * S, c - 25 * S, c + 25 * S, c + 25 * S], fill=(255, 196, 54, 255))
    d.ellipse([c - 21.5 * S, c - 21.5 * S, c + 21.5 * S, c + 21.5 * S], fill=INK)
    grad = Image.new('RGBA', img.size, (0, 0, 0, 0)); gd = ImageDraw.Draw(grad)
    for i in range(int(21 * S), 0, -1):
        t = i / (21 * S)
        col = (int(30 + 40 * (1 - t)), int(46 + 60 * (1 - t)), int(110 + 80 * (1 - t)), 255)
        gd.ellipse([c - i, c - i - 1.5 * S * (1 - t), c + i, c + i - 1.5 * S * (1 - t)], fill=col)
    img.alpha_composite(grad)
    d = ImageDraw.Draw(img)
    d.arc([c - 23.3 * S, c - 23.3 * S, c + 23.3 * S, c + 23.3 * S], start=200, end=320, fill=(255, 240, 170, 255), width=int(1.4 * S))
    sw = swords()
    k = (34 * S) / max(sw.size)
    sw = sw.resize((int(sw.width * k), int(sw.height * k)), Image.LANCZOS)
    img.alpha_composite(sw, ((img.width - sw.width) // 2, (img.height - sw.height) // 2 + int(.5 * S)))
    return img

for name, fn in [('sword', sword), ('swords', swords), ('slash', slash), ('badge', badge), ('puff', puff), ('star', star), ('spark', spark), ('ring', ring), ('dot', dot), ('square', square),
                 ('glow', glow), ('shadow', shadow), ('rays', rays), ('bubble', bubble), ('trophy', trophy),
                 ('crown', crown), ('check', check), ('cross', cross), ('vignette', vignette)]:
    save(name, fn())
print(sorted(os.listdir(OUT)))
