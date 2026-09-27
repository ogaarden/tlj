"""
Lager teksturene til salene i slottet (assets/floor/*.png).
Kjør: python3 tools/gen_room_textures.py   (trenger numpy og pillow)

Alle flis-teksturene er sømløse (kan legges ved siden av hverandre uten skjøter).
Detaljene tegnes i dobbel oppløsning og skaleres ned, så kantene blir myke.
"""
import math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = "assets/floor/"
rng = np.random.default_rng(7)
random.seed(7)

def periodic_noise(n, scale, seed):
    """Sømløs støy: filtrert hvit støy i frekvensrommet (periodisk av natur). 0..1"""
    r = np.random.default_rng(seed)
    white = r.standard_normal((n, n))
    f = np.fft.fft2(white)
    ky = np.fft.fftfreq(n)[:, None]; kx = np.fft.fftfreq(n)[None, :]
    k = np.sqrt(kx * kx + ky * ky)
    filt = np.exp(-(k * n / scale) ** 2)
    out = np.real(np.fft.ifft2(f * filt))
    out = (out - out.min()) / (out.max() - out.min())
    return out

def lerp(a, b, t):
    return a + (b - a) * t

def colorize(t, c0, c1):
    c0 = np.array(c0, float); c1 = np.array(c1, float)
    return c0 + (c1 - c0) * t[..., None]

def wrap_draw(draw_fn, size):
    """Tegn noe flere ganger forskjøvet med størrelsen, så det går sømløst over kantene"""
    for dx in (-size, 0, size):
        for dy in (-size, 0, size):
            draw_fn(dx, dy)

def save(arr_or_img, name, size=None):
    img = arr_or_img if isinstance(arr_or_img, Image.Image) else Image.fromarray(np.clip(arr_or_img, 0, 255).astype(np.uint8))
    if size: img = img.resize((size, size), Image.LANCZOS)
    img.save(OUT + name)
    print("lagret", name, img.size)

# ---------------------------------------------------------------- GRESS
def grass():
    S = 512  # tegnes i 512, lagres i 256
    n1 = periodic_noise(S, 6, 1); n2 = periodic_noise(S, 28, 2)
    t = 0.65 * n1 + 0.35 * n2
    base = colorize(t, (58, 108, 52), (112, 158, 74))
    img = Image.fromarray(base.astype(np.uint8)).convert("RGB")
    d = ImageDraw.Draw(img)
    greens = [(46, 92, 42), (70, 125, 58), (98, 150, 70), (132, 178, 88), (160, 196, 104)]
    for _ in range(9000):
        x, y = random.uniform(0, S), random.uniform(0, S)
        a = random.uniform(-2.6, -0.5)          # Mest "oppover" i bildet
        L = random.uniform(5, 13)
        col = random.choice(greens)
        w = random.choice([1, 1, 2])
        def blade(dx, dy):
            d.line([(x + dx, y + dy), (x + dx + math.cos(a) * L, y + dy + math.sin(a) * L)], fill=col, width=w)
        wrap_draw(blade, S)
    # Små blomster og kløver her og der
    for _ in range(70):
        x, y = random.uniform(0, S), random.uniform(0, S)
        col = random.choice([(250, 250, 240), (255, 225, 90), (240, 170, 200)])
        def flower(dx, dy):
            for k in range(5):
                aa = k * 2 * math.pi / 5
                d.ellipse([x + dx + math.cos(aa) * 3 - 2, y + dy + math.sin(aa) * 3 - 2, x + dx + math.cos(aa) * 3 + 2, y + dy + math.sin(aa) * 3 + 2], fill=col)
            d.ellipse([x + dx - 1.5, y + dy - 1.5, x + dx + 1.5, y + dy + 1.5], fill=(250, 200, 60))
        wrap_draw(flower, S)
    save(img, "grass.png", 256)

# ---------------------------------------------------------------- GRUS
def gravel():
    S = 512
    n = periodic_noise(S, 10, 3)
    base = colorize(n, (170, 155, 125), (205, 192, 162))
    img = Image.fromarray(base.astype(np.uint8)).convert("RGB")
    d = ImageDraw.Draw(img)
    tones = [(214, 202, 176), (190, 176, 148), (160, 150, 135), (225, 218, 200), (178, 160, 130), (140, 132, 125)]
    for _ in range(2600):
        x, y = random.uniform(0, S), random.uniform(0, S)
        rx, ry = random.uniform(3, 9), random.uniform(2.5, 7)
        c = random.choice(tones)
        def pebble(dx, dy):
            cx, cy = x + dx, y + dy
            d.ellipse([cx - rx + 1.5, cy - ry + 2, cx + rx + 1.5, cy + ry + 2], fill=(120, 108, 90))    # Skygge
            d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=c)
            hl = tuple(min(255, v + 28) for v in c)
            d.ellipse([cx - rx * 0.6, cy - ry * 0.7, cx, cy - ry * 0.1], fill=hl)                     # Glans
        wrap_draw(pebble, S)
    save(img, "gravel.png", 256)

# ---------------------------------------------------------------- SKIFERHELLER
def slate():
    S = 512
    img = Image.new("RGB", (S, S), (40, 40, 48))
    arr = np.array(img).astype(float)
    noise = periodic_noise(S, 40, 4); noise2 = periodic_noise(S, 8, 5)
    rows = 4; rh = S // rows
    stones = []
    for r in range(rows):
        # Løpende forband: bredder som summerer til S, forskjøvet per rad
        widths = []
        left = S
        while left > 0:
            w = random.choice([128, 160, 192, 96])
            if left - w < 90 and left - w != 0: w = left
            widths.append(min(w, left)); left -= widths[-1]
        x = (r % 2) * 70
        for w in widths:
            stones.append((x, r * rh, w, rh))
            x += w
    d = ImageDraw.Draw(img)
    for (x, y, w, h) in stones:
        tone = random.uniform(-14, 14)
        col = np.array([88, 90, 104]) + tone + np.array([random.uniform(-6, 6), 0, random.uniform(-4, 8)])
        for dx in (-S, 0):
            for dy in (0,):
                xs, ys = x + dx, y + dy
                x0, x1 = int(max(0, xs + 3)), int(min(S, xs + w - 3))
                y0, y1 = int(ys + 3), int(ys + h - 3)
                if x1 <= x0: continue
                patch = noise[y0:y1, x0:x1, None] * 26 + noise2[y0:y1, x0:x1, None] * 14 - 20
                arr[y0:y1, x0:x1] = col + patch
                arr[y0:y0 + 3, x0:x1] += 22     # Lys kant oppe
                arr[y0:y1, x0:x0 + 3] += 14
                arr[y1 - 3:y1, x0:x1] -= 22     # Mørk kant nede
                arr[y0:y1, x1 - 3:x1] -= 14
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    for _ in range(26):                          # Sprekker
        x, y = random.uniform(0, S), random.uniform(0, S)
        pts = [(x, y)]
        for k in range(random.randint(3, 6)):
            x += random.uniform(-14, 14); y += random.uniform(-14, 14); pts.append((x, y))
        d.line(pts, fill=(52, 52, 62), width=2)
    for _ in range(500):                         # Støv og småsteiner
        x, y = random.uniform(0, S), random.uniform(0, S)
        d.point((x, y), fill=(130, 130, 140))
    save(img, "slate.png", 256)

# ---------------------------------------------------------------- BLOMSTERBED
def flowerbed():
    S = 512
    n = periodic_noise(S, 12, 6)
    base = colorize(n, (70, 48, 32), (104, 72, 46))
    img = Image.fromarray(base.astype(np.uint8)).convert("RGB")
    d = ImageDraw.Draw(img)
    for _ in range(420):                         # Blader
        x, y = random.uniform(0, S), random.uniform(0, S)
        def leaf(dx, dy):
            a = random.uniform(0, math.pi * 2)
            d.ellipse([x + dx - 7, y + dy - 3.5, x + dx + 7, y + dy + 3.5], fill=random.choice([(52, 110, 50), (74, 136, 62), (60, 120, 70)]))
        wrap_draw(leaf, S)
    palette = [(220, 40, 60), (255, 210, 60), (250, 250, 245), (150, 80, 200), (255, 130, 60), (240, 120, 170)]
    for _ in range(260):                         # Blomster med kronblader
        x, y = random.uniform(0, S), random.uniform(0, S)
        col = random.choice(palette); r = random.uniform(4, 7)
        def fl(dx, dy):
            for k in range(6):
                aa = k * math.pi / 3
                px, py = x + dx + math.cos(aa) * r, y + dy + math.sin(aa) * r
                d.ellipse([px - r * 0.6, py - r * 0.6, px + r * 0.6, py + r * 0.6], fill=col)
            d.ellipse([x + dx - r * 0.45, y + dy - r * 0.45, x + dx + r * 0.45, y + dy + r * 0.45], fill=(255, 220, 90))
        wrap_draw(fl, S)
    save(img, "flowerbed.png", 256)

# ---------------------------------------------------------------- LØPERE (vevd, med bord og medaljonger)
def runner(name, field, field_dark, border, accent, motif):
    """Tekstur på tvers (x) og langs (y) løperen. Gjentas langs løperen."""
    W, H = 512, 512
    img = Image.new("RGB", (W, H), field)
    d = ImageDraw.Draw(img)
    # Vev: fine striper
    arr = np.array(img).astype(float)
    weave = (np.sin(np.arange(W) * 1.6)[None, :] * 3 + np.sin(np.arange(H) * 1.3)[:, None] * 3)
    arr += weave[..., None] + periodic_noise(W, 30, 11)[..., None] * 14 - 7
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)); d = ImageDraw.Draw(img)
    # Bord langs begge sider: gullstripe, mørkt bånd med ruter, gullstripe
    for side in (0, 1):
        x0 = 0 if side == 0 else W - 84
        d.rectangle([x0, 0, x0 + 84, H], fill=field_dark)
        d.rectangle([x0 + 8, 0, x0 + 14, H], fill=border)
        d.rectangle([x0 + 70, 0, x0 + 76, H], fill=border)
        for k in range(0, H, 32):                # Rutekjede
            cx, cy = x0 + 42, k + 16
            d.polygon([(cx, cy - 13), (cx + 13, cy), (cx, cy + 13), (cx - 13, cy)], fill=border)
            d.polygon([(cx, cy - 7), (cx + 7, cy), (cx, cy + 7), (cx - 7, cy)], fill=accent)
    # Geometrisk mønster midt i feltet (ingen figurer): ruter i ruter og en liten rosett
    cx, cy = W // 2, H // 2
    d.polygon([(cx, cy - 150), (cx + 110, cy), (cx, cy + 150), (cx - 110, cy)], fill=border)
    d.polygon([(cx, cy - 138), (cx + 100, cy), (cx, cy + 138), (cx - 100, cy)], fill=field_dark)
    d.polygon([(cx, cy - 104), (cx + 76, cy), (cx, cy + 104), (cx - 76, cy)], fill=border)
    d.polygon([(cx, cy - 94), (cx + 68, cy), (cx, cy + 94), (cx - 68, cy)], fill=field)
    for k in range(4):                           # Fire små ruter som en rosett
        a = k * math.pi / 2
        px, py = cx + math.cos(a) * 34, cy + math.sin(a) * 46
        d.polygon([(px, py - 16), (px + 12, py), (px, py + 16), (px - 12, py)], fill=accent)
    d.polygon([(cx, cy - 14), (cx + 10, cy), (cx, cy + 14), (cx - 10, cy)], fill=border)
    # Små prikker i hjørnene av feltet (ved skjøten, så de møtes på neste flis)
    for (px, py) in [(cx - 150, 0), (cx + 150, 0), (cx - 150, H), (cx + 150, H), (cx, 0), (cx, H)]:
        d.ellipse([px - 10, py - 10, px + 10, py + 10], fill=border)
        d.ellipse([px - 5, py - 5, px + 5, py + 5], fill=accent)
    save(img, name, 256)

# ---------------------------------------------------------------- DELFTTEPPE (rundt)
def delft_rug():
    S = 2048
    CREAM = (242, 238, 228); COBALT = (36, 62, 152); LIGHT = (96, 128, 200); DARK = (22, 38, 105)
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S / 2
    def circ(r, fill): d.ellipse([c - r, c - r, c + r, c + r], fill=fill)
    circ(1020, CREAM); circ(990, COBALT); circ(930, CREAM)
    # Bølgebord (skjell) mellom 930 og 830
    circ(920, LIGHT)
    for k in range(72):
        a = k * 2 * math.pi / 72
        x, y = c + math.cos(a) * 880, c + math.sin(a) * 880
        d.ellipse([x - 40, y - 40, x + 40, y + 40], fill=COBALT)
        d.ellipse([x - 20, y - 20, x + 20, y + 20], fill=CREAM)
    circ(820, CREAM); circ(800, COBALT)
    # Blomsterkrans
    for k in range(24):
        a = k * 2 * math.pi / 24
        x, y = c + math.cos(a) * 690, c + math.sin(a) * 690
        for p in range(6):
            aa = a + p * math.pi / 3
            px, py = x + math.cos(aa) * 34, y + math.sin(aa) * 34
            d.ellipse([px - 26, py - 26, px + 26, py + 26], fill=CREAM)
        d.ellipse([x - 20, y - 20, x + 20, y + 20], fill=LIGHT)
        # Ranker mellom blomstene
        a2 = a + math.pi / 24
        for s in (-1, 1):
            lx, ly = c + math.cos(a2) * (690 + s * 40), c + math.sin(a2) * (690 + s * 40)
            d.ellipse([lx - 18, ly - 9, lx + 18, ly + 9], fill=LIGHT)
    circ(580, CREAM); circ(560, DARK); circ(540, COBALT)
    # Stjerne i midten med 8 spisser
    pts = []
    for k in range(16):
        a = k * math.pi / 8 - math.pi / 2
        r = 500 if k % 2 == 0 else 210
        pts.append((c + math.cos(a) * r, c + math.sin(a) * r))
    d.polygon(pts, fill=CREAM)
    pts2 = [(c + (x - c) * 0.72, c + (y - c) * 0.72) for (x, y) in pts]
    d.polygon(pts2, fill=LIGHT)
    circ(170, CREAM); circ(140, COBALT)
    for k in range(8):
        a = k * math.pi / 4
        x, y = c + math.cos(a) * 80, c + math.sin(a) * 80
        d.ellipse([x - 38, y - 38, x + 38, y + 38], fill=CREAM)
    circ(44, (230, 185, 70))
    # Penselstrøk: litt ujevn farge, så det ser malt ut
    arr = np.array(img).astype(float)
    n = periodic_noise(S // 4, 20, 9)
    n = np.array(Image.fromarray((n * 255).astype(np.uint8)).resize((S, S), Image.BILINEAR)).astype(float) / 255
    arr[..., :3] *= (0.93 + 0.12 * n)[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")
    img = img.resize((1024, 1024), Image.LANCZOS)
    img.save(OUT + "delft_rug.png"); print("lagret delft_rug.png", img.size)

# ---------------------------------------------------------------- MARMORMEDALJONG (Storsalen)
def medallion():
    S = 2048
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S / 2
    GOLD = (222, 178, 64); WHITE = (236, 230, 220); GREY = (150, 146, 150); BLACK = (46, 44, 52); RED = (140, 30, 40)
    def circ(r, fill): d.ellipse([c - r, c - r, c + r, c + r], fill=fill)
    circ(1020, GOLD); circ(995, BLACK); circ(960, GOLD); circ(945, WHITE)
    # Ring med vekslende marmorfelt
    for k in range(32):
        a0 = k * 2 * math.pi / 32; a1 = (k + 1) * 2 * math.pi / 32
        col = GREY if k % 2 == 0 else RED
        pts = [(c + math.cos(a0) * 930, c + math.sin(a0) * 930), (c + math.cos(a1) * 930, c + math.sin(a1) * 930),
               (c + math.cos(a1) * 790, c + math.sin(a1) * 790), (c + math.cos(a0) * 790, c + math.sin(a0) * 790)]
        d.polygon(pts, fill=col)
    circ(780, GOLD); circ(765, WHITE)
    # Kompassrose: 16 stråler i to lengder, delt i lys/mørk halvdel
    for k in range(16):
        a = k * math.pi / 8 - math.pi / 2
        L = 740 if k % 2 == 0 else 480
        w = 0.17 if k % 2 == 0 else 0.13
        tip = (c + math.cos(a) * L, c + math.sin(a) * L)
        l = (c + math.cos(a - w) * 150, c + math.sin(a - w) * 150)
        r = (c + math.cos(a + w) * 150, c + math.sin(a + w) * 150)
        d.polygon([(c, c), l, tip], fill=BLACK if k % 2 == 0 else GREY)
        d.polygon([(c, c), tip, r], fill=GOLD if k % 2 == 0 else WHITE)
    circ(190, GOLD); circ(170, RED); circ(110, GOLD); circ(60, WHITE)
    # Marmorårer
    arr = np.array(img).astype(float)
    n = periodic_noise(S // 4, 60, 12)
    n = np.array(Image.fromarray((n * 255).astype(np.uint8)).resize((S, S), Image.BICUBIC)).astype(float) / 255
    veins = np.abs(np.sin(n * 40.0))
    arr[..., :3] *= (0.88 + 0.12 * veins)[..., None]
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA").resize((1024, 1024), Image.LANCZOS)
    img.save(OUT + "medallion.png"); print("lagret medallion.png", img.size)

grass(); gravel(); slate(); flowerbed()
runner("runner_red.png", (150, 28, 40), (105, 18, 30), (226, 182, 70), (40, 60, 140), "crown")
runner("runner_blue.png", (38, 60, 150), (24, 38, 105), (238, 234, 222), (160, 30, 45), "lily")
delft_rug(); medallion()
