"""
Generate viewcam.ico from the monogram.svg design.
The SVG uses no external fonts/images so we can approximate it
geometrically with Pillow's ImageDraw.
Design: dark oval body, purple iris gradient, dark pupil, white highlight, red LED.
"""
import math
from PIL import Image, ImageDraw, ImageFilter

def lerp_color(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))

def draw_icon(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d   = ImageDraw.Draw(img)
    s   = size / 64.0  # scale factor (SVG viewBox is 64x64)

    # --- Outer oval (dark body) ---
    # SVG path approximates an ellipse from (4,14) to (60,50)
    ox0, oy0 = 4*s, 14*s
    ox1, oy1 = 60*s, 50*s
    d.ellipse([ox0, oy0, ox1, oy1], fill=(11, 11, 14, 255))

    # --- Outer oval border: linear gradient approximated by a stroke ring ---
    # We'll use a slightly inset ellipse with a gold→purple→navy ring
    steps = 48
    cx, cy = 32*s, 32*s
    rw, rh = (ox1-ox0)/2, (oy1-oy0)/2
    # Colors: gold=(245,197,107), purple=(139,124,255), navy=(63,45,194)
    gold   = (245, 197, 107, 220)
    purple = (139, 124, 255, 220)
    navy   = (63,  45, 194, 200)
    bw = max(1, int(1.5*s))
    for i in range(steps):
        angle = 2 * math.pi * i / steps
        t_gold_purple = min(1.0, i / (steps * 0.5))
        if i < steps * 0.5:
            col = lerp_color(gold, purple, t_gold_purple)
        else:
            col = lerp_color(purple, navy, (i - steps*0.5) / (steps*0.5))
        px = cx + rw * math.cos(angle)
        py = cy + rh * math.sin(angle)
        d.ellipse([px-bw, py-bw, px+bw, py+bw], fill=col)

    # --- Iris: radial gradient (B7A7FF → 8B7CFF → 1A1240) ---
    # Approximate with concentric circles from center outward
    ir = 13*s
    iris_steps = max(8, int(ir))
    for ri in range(iris_steps, 0, -1):
        t = 1.0 - ri / iris_steps
        if t < 0.36:
            c = lerp_color((183,167,255,255), (139,124,255,255), t/0.36)
        else:
            c = lerp_color((139,124,255,255), (26,18,64,255),  (t-0.36)/0.64)
        r_now = ri * ir / iris_steps
        d.ellipse([cx-r_now, cy-r_now, cx+r_now, cy+r_now], fill=c)

    # --- Pupil (inner dark circle) ---
    pr = 8*s
    d.ellipse([cx-pr, cy-pr, cx+pr, cy+pr], fill=(6, 5, 14, 217))
    # thin purple ring
    d.ellipse([cx-pr, cy-pr, cx+pr, cy+pr], outline=(139,124,255,180), width=max(1,int(0.75*s)))

    # --- Deep pupil center ---
    dp = 3.5*s
    d.ellipse([cx-dp, cy-dp, cx+dp, cy+dp], fill=(4, 3, 18, 255))

    # --- Specular highlight ---
    hx, hy, hr = 29*s, 29*s, 1.6*s
    d.ellipse([hx-hr, hy-hr, hx+hr, hy+hr], fill=(255,255,255,217))

    # --- Red LED dot (top-right) ---
    lx, ly, lr = 48*s, 20*s, 2*s
    d.ellipse([lx-lr, ly-lr, lx+lr, ly+lr], fill=(255,107,107,255))

    # Slight blur for anti-aliasing at small sizes
    if size <= 32:
        img = img.filter(ImageFilter.SMOOTH)

    return img

sizes = [256, 48, 32, 16]
frames = [draw_icon(sz) for sz in sizes]
out = "resources/images/viewcam.ico"
frames[0].save(out, format="ICO", sizes=[(sz, sz) for sz in sizes],
               append_images=frames[1:])
print(f"Generated {out} with sizes {sizes}")
