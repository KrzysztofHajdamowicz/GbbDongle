#!/usr/bin/env python3
"""Generate the RJ45 / terminal-block pinout diagrams for the website.

Every RJ45 plug is drawn in one fixed orientation, the convention used on the
whole site: contacts facing the viewer, locking latch at the back, contact end
(pins) on the left, cable leaving on the right. In that view pin 1 is at the
bottom and pin 8 at the top.

Output goes to website/static/img/pinouts/. Run from the repo root:

    python3 tools/pinouts/gen_svg.py

Standard library only; the SVGs are plain text so diffs stay reviewable.
"""

from __future__ import annotations

import re
from pathlib import Path
from xml.sax.saxutils import escape

OUT = Path(__file__).resolve().parents[2] / "website" / "static" / "img" / "pinouts"

# Plug artwork for the convention drawing; see the header of that file for its
# source. Contact centres in the artwork's own units: pin 1, then one per pitch.
PLUG_ART = Path(__file__).with_name("rj45-t568b-plug.svg")
PLUG_ART_CREDIT = (
    'based on "RJ-45 TIA-568B Right.svg" by Miha Ulanov, Wikimedia Commons, public domain, '
    "https://commons.wikimedia.org/wiki/File:RJ-45_TIA-568B_Right.svg"
)
PLUG_PIN1_Y = 5.993
PLUG_PIN_PITCH = 3.296

# Signal roles -> label chip colours (light background, white-blue site).
ROLE = {
    "A": ("#1d4ed8", "#ffffff"),  # RS485 A
    "B": ("#c2410c", "#ffffff"),  # RS485 B
    "GND": ("#374151", "#ffffff"),
    "other": ("#e5e7eb", "#374151"),  # used by something else (CAN, meter, ...)
    "nc": ("#f3f4f6", "#9ca3af"),  # not connected / reserved
}

# T568B wire colours, pin 1..8: (base colour, striped with white?)
T568B = {
    1: ("#f97316", True),
    2: ("#f97316", False),
    3: ("#16a34a", True),
    4: ("#2563eb", False),
    5: ("#2563eb", True),
    6: ("#16a34a", False),
    7: ("#92400e", True),
    8: ("#92400e", False),
}

FONT = "font-family=\"Inter, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif\""


def _chip(x: float, y: float, text: str, role: str, anchor_right: bool = True) -> str:
    fill, fg = ROLE[role]
    w = max(56, 8.2 * len(text) + 20)
    x0 = x - w if anchor_right else x
    weight = "700" if role in ("A", "B", "GND") else "500"
    return (
        f'<rect x="{x0:.1f}" y="{y - 11:.1f}" width="{w:.1f}" height="22" rx="11" fill="{fill}"'
        + (' stroke="#d1d5db"' if role in ("other", "nc") else "")
        + "/>"
        f'<text x="{x0 + w / 2:.1f}" y="{y + 4.5:.1f}" text-anchor="middle" font-size="13" '
        f'font-weight="{weight}" fill="{fg}" {FONT}>{escape(text)}</text>'
    )


def plug_svg(pins: dict[int, tuple[str, str]], title: str) -> str:
    """RJ45 plug in the site convention. pins: {1..8: (label, role)}."""
    W, H = 640, 300
    tip_x, body_r = 236, 452  # plug body spans tip_x..body_r
    top, bot = 46, 254
    pitch = 22
    # pin 8 at the top, pin 1 at the bottom
    ys = {n: bot - 24 - (n - 1) * pitch for n in range(1, 9)}

    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
        f'role="img" aria-labelledby="t">',
        f"<title id=\"t\">{escape(title)}</title>",
        f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
        # cable
        f'<rect x="{body_r + 40}" y="122" width="{W - body_r - 40}" height="56" fill="#cbd5e1"/>',
        f'<rect x="{body_r}" y="112" width="44" height="76" rx="8" fill="#94a3b8"/>',
        # plug body (clear plastic)
        f'<rect x="{tip_x}" y="{top}" width="{body_r - tip_x}" height="{bot - top}" rx="10" '
        f'fill="#eff6ff" stroke="#60a5fa" stroke-width="2"/>',
        # hidden latch at the back: dashed outline
        f'<path d="M{tip_x + 70},{(top + bot) / 2 - 26} L{body_r - 20},{(top + bot) / 2 - 18} '
        f'L{body_r - 20},{(top + bot) / 2 + 18} L{tip_x + 70},{(top + bot) / 2 + 26} Z" '
        f'fill="none" stroke="#93c5fd" stroke-width="1.6" stroke-dasharray="6 4"/>',
    ]
    # wires from contacts to the cable
    for n in range(1, 9):
        y = ys[n]
        cy = 150 + (n - 4.5) * 5
        colour, striped = T568B[n]
        d = f"M{tip_x + 64},{y} C{tip_x + 150},{y} {body_r - 70},{cy} {body_r + 2},{cy}"
        out.append(f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="4"/>')
        if striped:
            out.append(
                f'<path d="{d}" fill="none" stroke="#ffffff" stroke-width="4" stroke-dasharray="5 5"/>'
            )
    # gold contacts at the tip
    for n in range(1, 9):
        y = ys[n]
        used = pins[n][1] in ("A", "B", "GND")
        out.append(
            f'<rect x="{tip_x + 6}" y="{y - 6}" width="58" height="12" rx="2" '
            f'fill="{"#eab308" if used else "#fde68a"}" stroke="#a16207" stroke-width="1"/>'
        )
        out.append(
            f'<text x="{tip_x - 14}" y="{y + 4.5}" text-anchor="middle" font-size="14" '
            f'font-weight="700" fill="#1e3a8a" {FONT}>{n}</text>'
        )
        label, role = pins[n]
        out.append(_chip(tip_x - 30, y, label, role))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def socket_svg(latch_up: bool, title: str) -> str:
    """Front view of an RJ45 socket (looking into the hole) with the latch slot
    at the bottom (usual) or at the top (socket mounted upside down)."""
    W, H = 260, 240
    x0, x1, y0, y1 = 30, 230, 40, 200
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
        f'role="img" aria-labelledby="t"><title id="t">{escape(title)}</title>',
        f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
        f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" rx="6" fill="#1f2937"/>',
        f'<rect x="{x0 + 14}" y="{y0 + 14}" width="{x1 - x0 - 28}" height="{y1 - y0 - 28}" rx="3" fill="#0b1220"/>',
    ]
    # latch slot
    sy = (y1 - 14 - 34) if not latch_up else (y0 + 14)
    out.append(f'<rect x="{(x0 + x1) / 2 - 34}" y="{sy}" width="68" height="34" fill="#475569"/>')
    # contacts along the side opposite to the latch slot
    cy0 = (y0 + 22) if not latch_up else (y1 - 22 - 40)
    left_to_right = list(range(1, 9)) if not latch_up else list(range(8, 0, -1))
    for i, n in enumerate(left_to_right):
        cx = x0 + 34 + i * 19
        colour = "#1d4ed8" if n == 1 else "#eab308"
        out.append(f'<rect x="{cx}" y="{cy0}" width="6" height="40" fill="{colour}"/>')
        ty = (y0 - 10) if not latch_up else (y1 + 26)
        out.append(
            f'<text x="{cx + 3}" y="{ty}" text-anchor="middle" font-size="13" '
            f'font-weight="{"800" if n == 1 else "500"}" fill="{"#1d4ed8" if n == 1 else "#374151"}" {FONT}>{n}</text>'
        )
    out.append("</svg>")
    return "\n".join(out) + "\n"


def hoymiles_block_svg() -> str:
    """Hoymiles HiT COM1: 2x3 stacked RJ45 block, front view. The top row is
    mounted upside down (latch slot at the top), per the HIT-(5-20)L-G3 manual."""
    W, H = 600, 330
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
        f'role="img" aria-labelledby="t"><title id="t">Hoymiles HiT COM1 port block, front view</title>',
        f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
    ]
    names = [["Parallel_2", "Meter_2", "BMS"], ["Parallel_1", "Meter_1", "DRM"]]
    for r in range(2):
        for c in range(3):
            x = 30 + c * 190
            y = 40 + r * 140
            name = names[r][c]
            hl = name == "Meter_2"
            out.append(
                f'<rect x="{x}" y="{y}" width="160" height="110" rx="6" fill="#1f2937" '
                f'stroke="{"#2563eb" if hl else "#1f2937"}" stroke-width="{5 if hl else 1}"/>'
            )
            out.append(f'<rect x="{x + 12}" y="{y + 12}" width="136" height="86" rx="3" fill="#0b1220"/>')
            latch_up = r == 0
            sy = y + 12 if latch_up else y + 98 - 24
            out.append(f'<rect x="{x + 56}" y="{sy}" width="48" height="24" fill="#475569"/>')
            cy = y + 98 - 30 - 6 if latch_up else y + 18
            order = list(range(8, 0, -1)) if latch_up else list(range(1, 9))
            for i, n in enumerate(order):
                cx = x + 30 + i * 14
                out.append(
                    f'<rect x="{cx}" y="{cy}" width="5" height="30" fill="{"#1d4ed8" if n == 1 else "#eab308"}"/>'
                )
            # pin-1 marker
            p1x = x + 30 + (7 if latch_up else 0) * 14 + 2.5
            out.append(
                f'<text x="{p1x}" y="{y + 128 if latch_up else cy - 6}" text-anchor="middle" font-size="12" '
                f'font-weight="800" fill="#1d4ed8" {FONT}>1</text>'
            )
            ly = y - 10 if r == 0 else y + 110 + 22
            out.append(
                f'<text x="{x + 80}" y="{ly}" text-anchor="middle" font-size="15" '
                f'font-weight="{"800" if hl else "500"}" fill="{"#1d4ed8" if hl else "#374151"}" {FONT}>{name}</text>'
            )
    out.append("</svg>")
    return "\n".join(out) + "\n"


def terminal_svg(cells: list[tuple[str, str]], title: str) -> str:
    """Screw/spring terminal block, positions numbered left to right."""
    n = len(cells)
    pitch = 92
    W, H = 40 + n * pitch, 190
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
        f'role="img" aria-labelledby="t"><title id="t">{escape(title)}</title>',
        f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
        f'<rect x="20" y="30" width="{n * pitch}" height="80" rx="6" fill="#d1fae5" stroke="#059669" stroke-width="2"/>',
    ]
    for i, (label, role) in enumerate(cells):
        cx = 20 + pitch / 2 + i * pitch
        out.append(f'<circle cx="{cx}" cy="70" r="20" fill="#9ca3af" stroke="#4b5563" stroke-width="2"/>')
        out.append(f'<line x1="{cx - 13}" y1="70" x2="{cx + 13}" y2="70" stroke="#374151" stroke-width="4"/>')
        out.append(
            f'<text x="{cx}" y="22" text-anchor="middle" font-size="14" font-weight="700" fill="#1e3a8a" {FONT}>{i + 1}</text>'
        )
        out.append(_chip(cx - 42, 140, label, role, anchor_right=False))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def _plug_art() -> tuple[str, float, float]:
    """The plug artwork (one <g>) and its width and height in its own units."""
    src = PLUG_ART.read_text(encoding="utf-8")
    w, h = (float(v) for v in re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', src).groups())
    return src[src.index("<g") : src.rindex("</g>") + len("</g>")], w, h


def _arrow(x: float, y: float, to_right: bool) -> str:
    d = 9 if to_right else -9
    return f'<path d="M{x},{y - 6} L{x + d},{y} L{x},{y + 6} Z" fill="#1d4ed8"/>'


def conv_legend(lang: str) -> str:
    """How to hold the plug to match every diagram on the site."""
    t = {
        "pl": {
            "title": "Tak trzymaj wtyczkę RJ45",
            "top": "na górze",
            "bottom": "na dole",
            "pins": "piny po lewej",
            "cable": "kabel w prawo",
            "view": "Złote styki skierowane do Ciebie, zatrzask z tyłu (niewidoczny).",
            "wires": "Kolory przewodów według T568B.",
        },
        "en": {
            "title": "Hold the RJ45 plug like this",
            "top": "at the top",
            "bottom": "at the bottom",
            "pins": "pins on the left",
            "cable": "cable to the right",
            "view": "Gold contacts facing you, latch at the back (out of sight).",
            "wires": "Wire colours follow T568B.",
        },
    }[lang]
    art, art_w, art_h = _plug_art()
    W, H = 700, 372
    s = 5.6  # px per unit of the artwork
    ax, ay = 146, 70  # top-left corner of the placed artwork
    # The artwork has the cable on the left and pin 1 at the top. A half turn puts
    # the pins on the left and pin 1 at the bottom while still showing the contact
    # side; a mirror image would show the latch side instead.
    turn = f"translate({ax + s * art_w:.2f},{ay + s * art_h:.2f}) scale({-s})"
    pin_y = {n: ay + s * (art_h - (PLUG_PIN1_Y + (n - 1) * PLUG_PIN_PITCH)) for n in range(1, 9)}
    col = ax - 16  # centre of the pin-number column

    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
        f'role="img" aria-labelledby="t">',
        f'<title id="t">{escape(t["title"])}</title>',
        f"<!-- Plug artwork: {PLUG_ART_CREDIT} -->",
        f'<rect width="{W}" height="{H}" fill="#ffffff"/>',
        f'<text x="{W / 2}" y="38" text-anchor="middle" font-size="21" font-weight="800" fill="#1e3a8a" {FONT}>{escape(t["title"])}</text>',
        f'<g transform="{turn}">',
        art,
        "</g>",
        f'<text x="{col}" y="{pin_y[8] - 20:.1f}" text-anchor="middle" font-size="12" fill="#6b7280" {FONT}>pin</text>',
    ]
    for n in range(1, 9):
        y = pin_y[n]
        if n in (1, 8):
            out.append(f'<circle cx="{col}" cy="{y:.1f}" r="11" fill="#1d4ed8"/>')
            out.append(
                f'<text x="{col}" y="{y + 5.5:.1f}" text-anchor="middle" font-size="15" font-weight="800" '
                f'fill="#ffffff" {FONT}>{n}</text>'
            )
            out.append(
                f'<text x="{col - 20}" y="{y + 5:.1f}" text-anchor="end" font-size="14" font-weight="600" '
                f'fill="#1d4ed8" {FONT}>{escape(t["top" if n == 8 else "bottom"])}</text>'
            )
        else:
            out.append(
                f'<text x="{col}" y="{y + 5:.1f}" text-anchor="middle" font-size="14" font-weight="600" '
                f'fill="#374151" {FONT}>{n}</text>'
            )
    below = ay + s * art_h + 28
    right = ax + s * art_w
    out += [
        _arrow(ax + 9, below - 5, to_right=False),
        f'<text x="{ax + 18}" y="{below}" font-size="14" font-weight="600" fill="#1d4ed8" {FONT}>{escape(t["pins"])}</text>',
        _arrow(right - 9, below - 5, to_right=True),
        f'<text x="{right - 18:.1f}" y="{below}" text-anchor="end" font-size="14" font-weight="600" fill="#1d4ed8" {FONT}>{escape(t["cable"])}</text>',
        f'<text x="{W / 2}" y="{below + 36:.1f}" text-anchor="middle" font-size="14" fill="#374151" {FONT}>{escape(t["view"])}</text>',
        f'<text x="{W / 2}" y="{below + 58:.1f}" text-anchor="middle" font-size="12" fill="#6b7280" {FONT}>{escape(t["wires"])}</text>',
        "</svg>",
    ]
    return "\n".join(out) + "\n"


def nc(*ns: int) -> dict[int, tuple[str, str]]:
    return {n: ("NC", "nc") for n in ns}


DIAGRAMS: dict[str, tuple[str, dict[int, tuple[str, str]]]] = {
    # Deye SUN-(14-20)K-SG05LP3-EU-SM2 manual, Appendix I (p. 49): "Modbus port"
    "deye-modbus": (
        "Deye three-phase hybrid, Modbus port (SG04LP3 / SG05LP3)",
        {
            1: ("485 B", "B"), 2: ("485 A", "A"), 3: ("GND", "GND"),
            **nc(4, 5),
            6: ("GND", "GND"), 7: ("485 A", "other"), 8: ("485 B", "other"),
        },
    ),
    # same manual, "Meter-485 port"
    "deye-meter485": (
        "Deye three-phase hybrid, Meter-485 port",
        {
            1: ("485 B", "B"), 2: ("485 A", "A"), 3: ("COM-GND", "GND"),
            4: ("485 B", "other"), 5: ("485 A", "other"), 6: ("COM-GND", "other"),
            7: ("485 A", "other"), 8: ("485 B", "other"),
        },
    ),
    # Solis S6-EH3P(8-20)K(-LV)-ND-H manual V1.2 07/2025, 4.8.6 "RS485 Port"
    "solis-s6-eh3p": (
        "Solis S6-EH3P three-phase hybrid, RS485 port",
        {1: ("RS485 A", "A"), 2: ("RS485 B", "B"), **nc(3, 4, 5, 6, 7, 8)},
    ),
    # Solis S6-EH1P-L-PRO manual V1.1 03/2024 (3.10.6) and S6-EH1P-L-AU V1.1 (4.7.6)
    "solis-s6-eh1p": (
        "Solis S6-EH1P single-phase hybrid, RS485 port",
        {4: ("RS485 B", "B"), 5: ("RS485 A", "A"), **nc(1, 2, 3, 6, 7, 8)},
    ),
    # Hoymiles HIT-(5-20)L-G3 manual, 7.7 COM1: Meter terminals
    "hoymiles-hit-meter2": (
        "Hoymiles HiT-G3, Meter_2 port",
        {4: ("485 A", "A"), 5: ("485 B", "B"), 6: ("GND", "GND"), **nc(1, 2, 3, 7, 8)},
    ),
    # Felicity T-REX-50KHP3G01 RS485 port (manufacturer table via SolarAssistant)
    "felicity-trex-hp-rs485": (
        "Felicity T-REX HP (e.g. 50KHP3G01), RS485 port",
        {
            1: ("485 A2 (meter)", "other"), 2: ("485 B2 (meter)", "other"),
            3: ("485 A3", "A"), 4: ("485 B3", "B"),
            5: ("485 B3", "other"), 6: ("485 A3", "other"),
            7: ("RY_4", "other"), 8: ("RY_5", "other"),
        },
    ),
}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    files = {}
    for name, (title, pins) in DIAGRAMS.items():
        assert sorted(pins) == list(range(1, 9)), name
        files[f"{name}.svg"] = plug_svg(pins, title)
    files["convention-pl.svg"] = conv_legend("pl")
    files["convention-en.svg"] = conv_legend("en")
    files["socket-latch-down.svg"] = socket_svg(False, "RJ45 socket, front view, latch slot at the bottom")
    files["socket-latch-up.svg"] = socket_svg(True, "RJ45 socket, front view, latch slot at the top")
    files["hoymiles-hit-com1.svg"] = hoymiles_block_svg()
    # Felicity T-REX-10KLP3G01 user manual (DE, 358-S010004-01), table 4.4-1 COM port
    files["felicity-trex-lp3-com.svg"] = terminal_svg(
        [("RY1_4", "other"), ("RY1_5", "other"), ("485 B3", "B"), ("485 A3", "A"),
         ("485 A2", "other"), ("485 B2", "other")],
        "Felicity T-REX-10KLP3G01 COM port",
    )
    for fname, svg in files.items():
        (OUT / fname).write_text(svg, encoding="utf-8")
        print(OUT / fname)


if __name__ == "__main__":
    main()
