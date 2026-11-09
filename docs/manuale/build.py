#!/usr/bin/env python3
"""Genera il manuale d'uso di CARTILAGINE.

    python3 build.py            -> manuale.html e CARTILAGINE-Manuale-d-uso.pdf

Le impostazioni delle ricette si scrivono nel sorgente come
    <knobs>SPEED=7 DEPTH=10 BIAS=3 GAIN=10 LEVEL=8 WAVE=0 MODE=VCA</knobs>
e diventano una fila di manopole disegnate in SVG. Il PDF si stampa con Chrome senza interfaccia.
"""
import math, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "manuale.src.html"
OUT_HTML = HERE / "manuale.html"
OUT_PDF = HERE / "CARTILAGINE-Manuale-d-uso.pdf"

WAVES = ["rampa su", "rampa giù", "triangolo", "quadra"]


def wave_path(i, x0, y0, w, h):
    """Simbolo della forma d'onda (come sul pannello) dentro il riquadro x0, y0, w, h."""
    b, t, p = y0 + h, y0, w / 2
    if i == 0:
        pts = [(x0, b), (x0 + p, t), (x0 + p, b), (x0 + 2 * p, t), (x0 + 2 * p, b)]
    elif i == 1:
        pts = [(x0, b), (x0, t), (x0 + p, b), (x0 + p, t), (x0 + 2 * p, b)]
    elif i == 2:
        pts = [(x0, b), (x0 + p / 2, t), (x0 + p, b), (x0 + 1.5 * p, t), (x0 + 2 * p, b)]
    else:
        pts = [(x0, b), (x0, t), (x0 + p / 2, t), (x0 + p / 2, b), (x0 + p, b), (x0 + p, t),
               (x0 + 1.5 * p, t), (x0 + 1.5 * p, b), (x0 + 2 * p, b)]
    return "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)


def knob_svg(label, value):
    """Manopola 0-10 su 270 gradi, con l'indice sul valore."""
    v = float(value)
    a = math.radians(-135 + 270 * v / 10)
    cx, cy, r = 32, 30, 15
    ticks = []
    for k in range(11):
        t = math.radians(-135 + 27 * k)
        r1, r2 = (r + 4, r + 8) if k % 5 else (r + 3.5, r + 9.5)
        ticks.append(f'<line x1="{cx + r1 * math.sin(t):.1f}" y1="{cy - r1 * math.cos(t):.1f}" '
                     f'x2="{cx + r2 * math.sin(t):.1f}" y2="{cy - r2 * math.cos(t):.1f}" class="tk"/>')
    px, py = cx + (r - 3) * math.sin(a), cy - (r - 3) * math.cos(a)
    qx, qy = cx + 4 * math.sin(a), cy - 4 * math.cos(a)
    vtxt = f"{v:g}".replace(".", ",")
    return (f'<figure class="kn"><svg viewBox="0 0 64 60">{"".join(ticks)}'
            f'<circle cx="{cx}" cy="{cy}" r="{r}" class="body"/>'
            f'<line x1="{qx:.1f}" y1="{qy:.1f}" x2="{px:.1f}" y2="{py:.1f}" class="ptr"/></svg>'
            f'<figcaption><b>{label}</b> {vtxt}</figcaption></figure>')


def wave_svg(i):
    i = int(i)
    sym = "".join(
        f'<path d="{wave_path(k, 4 + k * 15, 4, 11, 7)}" class="{"on" if k == i else "off"}"/>' for k in range(4))
    return (f'<figure class="kn wv"><svg viewBox="0 0 64 60"><g transform="translate(0,6)">{sym}</g>'
            f'<path d="{wave_path(i, 14, 30, 36, 18)}" class="big"/></svg>'
            f'<figcaption><b>FORMA</b> {WAVES[i]}</figcaption></figure>')


def mode_svg(mode):
    up = mode.upper() == "VCA"
    y = 14 if up else 46
    return (f'<figure class="kn sw"><svg viewBox="0 0 64 60">'
            f'<circle cx="32" cy="30" r="7" class="nut"/><line x1="32" y1="30" x2="32" y2="{y}" class="lever"/>'
            f'<circle cx="32" cy="{y}" r="3.6" class="tip"/>'
            f'<text x="46" y="16" class="{"on" if up else "off"}">VCA</text>'
            f'<text x="46" y="50" class="{"off" if up else "on"}">VCF</text></svg>'
            f'<figcaption><b>{mode.upper()}</b></figcaption></figure>')


def knobs(spec):
    parts = []
    for item in spec.split():
        k, v = item.split("=")
        if k == "WAVE":
            parts.append(wave_svg(v))
        elif k == "MODE":
            parts.append(mode_svg(v))
        else:
            parts.append(knob_svg(k, v))
    return '<div class="knobs">' + "".join(parts) + "</div>"


def bolt_svg(cls="bolt"):
    # lo stesso fulmine del pannello, a punta alle due estremità
    return (f'<svg class="{cls}" viewBox="0 0 130 232" preserveAspectRatio="none">'
            '<path d="M130,0 L80,104 L106,104 L0,232 L50,128 L24,128 Z"/></svg>')


def main():
    html = SRC.read_text(encoding="utf-8")
    html = re.sub(r"<knobs>(.*?)</knobs>", lambda m: knobs(m.group(1)), html, flags=re.S)
    html = html.replace("<bolt/>", bolt_svg()).replace("<bolt-small/>", bolt_svg("bolt-small"))
    OUT_HTML.write_text(html, encoding="utf-8")

    chrome = shutil.which("google-chrome") or shutil.which("chromium") or shutil.which("chromium-browser")
    if chrome is None:
        sys.exit("serve Chrome o Chromium per stampare il PDF")
    subprocess.run([chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                    "--run-all-compositor-stages-before-draw", "--virtual-time-budget=10000",
                    f"--print-to-pdf={OUT_PDF}", OUT_HTML.as_uri()], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(OUT_PDF)


if __name__ == "__main__":
    main()
