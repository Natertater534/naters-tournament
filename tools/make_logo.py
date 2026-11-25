"""The mod's logo (336 x 336): navy tile with a gold rim, the mod's own crossed swords,
and Nater's handshake emote in front. Run: python3 tools/make_logo.py -> logo.png
"""
import math, os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(__file__)
ROOT = os.path.join(HERE, '..')
src = open(os.path.join(HERE, 'make_art.py')).read().split('\nfor name, fn in [')[0]
art = {'__file__': os.path.join(HERE, 'make_art.py')}
exec(src.replace("S = 4  # uhd scale", "S = 16"), art)   # draw the sword big, then scale down

N = 336
img = Image.new('RGBA', (N, N), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
pad, r = 8, 54
# gold rim, dark edge, navy face with a soft vertical gradient
d.rounded_rectangle([pad - 4, pad - 4, N - pad + 4, N - pad + 4], radius=r + 4, fill=(16, 20, 40, 255))
d.rounded_rectangle([pad, pad, N - pad, N - pad], radius=r, fill=(255, 196, 54, 255))
face = Image.new('RGBA', (N, N), (0, 0, 0, 0))
fd = ImageDraw.Draw(face)
inset = pad + 14
for i in range(N):
    t = i / N
    fd.line([(0, i), (N, i)], fill=(int(22 + 18 * t), int(34 + 26 * t), int(84 + 40 * t), 255))
mask = Image.new('L', (N, N), 0)
ImageDraw.Draw(mask).rounded_rectangle([inset, inset, N - inset, N - inset], radius=r - 12, fill=255)
img.paste(face, (0, 0), mask)
d = ImageDraw.Draw(img)
d.rounded_rectangle([inset, inset, N - inset, N - inset], radius=r - 12, outline=(16, 20, 40, 255), width=4)

# crossed swords, crossing behind the emote
one = art['sword_img']()
k = 292 / one.height
one = one.resize((int(one.width * k), int(one.height * k)), Image.LANCZOS)
pivot = (one.width / 2, one.height * .5)
side = int(one.height * 1.6)
swords = Image.new('RGBA', (side, side), (0, 0, 0, 0))
for ang in (38, -38):
    layer = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    layer.alpha_composite(one, (int(side / 2 - pivot[0]), int(side / 2 - pivot[1])))
    swords.alpha_composite(layer.rotate(ang, resample=Image.BICUBIC, center=(side / 2, side / 2)))
shadow = Image.new('RGBA', swords.size, (0, 0, 0, 0))
shadow.putalpha(swords.split()[3].point(lambda v: v * .45))
shadow = shadow.filter(ImageFilter.GaussianBlur(6))
cx, cy = N // 2, N // 2 + 4
img.alpha_composite(shadow, (cx - side // 2 + 4, cy - side // 2 + 8))
img.alpha_composite(swords, (cx - side // 2, cy - side // 2))

# Nater's handshake emote (it carries its own glow)
emote = Image.open(os.path.join(ROOT, 'resources', 'emotes', 'emote-04-uhd.png')).convert('RGBA')
ek = 200 / max(emote.size)
emote = emote.resize((int(emote.width * ek), int(emote.height * ek)), Image.LANCZOS)
img.alpha_composite(emote, (cx - emote.width // 2, cy - emote.height // 2 + 8))
img.save(os.path.join(ROOT, 'logo.png'), optimize=True)
print('logo.png', img.size)
