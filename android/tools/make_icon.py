#!/usr/bin/env python3
"""Pixel-art Sonic head launcher icon, Genesis style.

The sprite is drawn with simple shapes on a tiny canvas (so it keeps a
chunky pixel look after nearest-neighbour upscale) and a 1px black outline
is generated automatically by dilating the alpha mask.
"""
from PIL import Image, ImageDraw
import sys

W, H = 32, 28
BLUE = (36, 84, 200, 255)
DARK = (24, 56, 150, 255)
SKIN = (240, 178, 122, 255)
WHITE = (245, 245, 250, 255)
BLACK = (14, 14, 22, 255)

spr = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(spr)

# head ball, right of centre
d.ellipse([8, 2, 26, 20], fill=BLUE)
# three back spikes, pointing left and down-left (chunky triangles)
d.polygon([(13, 3), (2, 8), (14, 12)], fill=BLUE)
d.polygon([(11, 9), (0, 15), (13, 16)], fill=BLUE)
d.polygon([(12, 14), (4, 22), (16, 19)], fill=BLUE)
# jaw / cheek toward the muzzle
d.ellipse([13, 11, 25, 21], fill=BLUE)
# shading along the lower-left of the head
d.ellipse([10, 12, 18, 20], fill=DARK)
d.ellipse([12, 11, 22, 19], fill=BLUE)
# ear nub on top
d.polygon([(12, 3), (14, 0), (16, 3)], fill=BLUE)
# eye: classic side-view white patch with the pupil looking forward
d.ellipse([19, 4, 27, 15], fill=WHITE)
d.rectangle([25, 10, 26, 13], fill=BLACK)
# muzzle, higher and smaller
d.ellipse([20, 13, 29, 19], fill=SKIN)
# nose tip at the point of the muzzle
d.rectangle([28, 12, 30, 14], fill=BLACK)

# auto 1px black outline: dilate alpha, subtract original
mask = spr.split()[3].point(lambda a: 255 if a > 0 else 0)
dil = mask.copy()
for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
    shifted = Image.new("L", (W, H), 0)
    shifted.paste(mask, (dx, dy))
    dil = Image.composite(Image.new("L", (W, H), 255), dil, shifted)
outline_mask = Image.composite(Image.new("L", (W, H), 0), dil, mask)
outline = Image.new("RGBA", (W, H), (0, 0, 0, 0))
outline.paste(BLACK, (0, 0), outline_mask)
sprite = Image.alpha_composite(outline, spr)

# Background: Genesis deep blue rounded square with a gold ring behind Sonic.
SIZE = 480
img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
d2 = ImageDraw.Draw(img)
d2.rounded_rectangle([0, 0, SIZE - 1, SIZE - 1], radius=SIZE // 5,
                     fill=(10, 16, 56, 255))
cx, cy, r0, r1 = SIZE // 2, SIZE // 2, SIZE // 3, SIZE // 3 - SIZE // 14
d2.ellipse([cx - r0, cy - r0, cx + r0, cy + r0], fill=(255, 200, 30, 255))
d2.ellipse([cx - r1, cy - r1, cx + r1, cy + r1], fill=(10, 16, 56, 255))

scale = int(SIZE * 0.80) // W
sw, sh = W * scale, H * scale
spr_big = sprite.resize((sw, sh), Image.NEAREST)
img.alpha_composite(spr_big, ((SIZE - sw) // 2, (SIZE - sh) // 2))

out = sys.argv[1] if len(sys.argv) > 1 else "icon_preview.png"
img.save(out)
print("wrote", out)
