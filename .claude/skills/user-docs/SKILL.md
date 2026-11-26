---
name: user-docs
description: Use when changing anything GbbDongle users read — the FAQ (docs/faq.md, docs/faq.pl.md), the pairing guide, product-site pages under website/, pinout pages and drawings, the price — or when turning an answer Krzysztof gave on Discord into documentation.
---

# User-facing docs

Users read these on the site, on GitHub and through the Discord support bot,
which answers from the FAQ and the pairing guide. A wrong sentence here is repeated to every
user who asks. Most docs exist in English and Polish; **change both in the
same commit**.

## Content rules

- Write only what is confirmed: by the code, by Krzysztof, or by Gbbsoft for
  GbbOptimizer behaviour. Keep anything else out, or mark it and list it under
  "Needs confirmation" in the PR description.
- A recurring question Krzysztof answered on Discord is the best source: keep
  his facts, rewrite the wording, never quote users or name them.
- Link third-party documentation (Solar Assistant's inverter pinouts,
  inverter manuals); don't copy it.
- Never put internal material in user docs: test reports, compliance work,
  plans, anything from `testing-campaign-2026-10` or
  `milestone/en303645-compliance`.

## Where things live

| Content | EN | PL | Notes |
|---|---|---|---|
| FAQ | `docs/faq.md` | `docs/faq.pl.md` | Rendered at `/faq/` and `/en/faq/` by `website/layouts/_default/faq.html`, which strips the first `# heading` and the paragraph after it: keep that intro. Sections are anchors the bot links to; don't rename headings casually |
| Pairing guide | `docs/pairing.md` | `docs/pairing.pl.md` | Screenshots in `docs/img/pairing/` must mask the pairing code and the Plant Id |
| Site pages | `website/content/en/<name>.md` | `website/content/pl/<name>.md` | Same file name; the EN page sets a `slug` (`kup`→`buy`, `instalacja`→`install`, `urzadzenia`→`devices`) |
| Pinouts | `website/content/en/pinouts/` | `website/content/pl/pinouty/` | Different folders, paired by `translationKey` (`pinouts-<brand>`) |
| UI strings | `website/i18n/en.yaml` | `website/i18n/pl.yaml` | Same keys in both |
| Boards | `website/data/devices.yaml` | (same file, `pl:`/`en:` keys) | See the `add-board` skill |

The price (399 PLN gross) appears in `website/hugo.toml`
(`[languages.*.params] price`) and as literal text in `content/{pl,en}/kup.md`
and `content/{pl,en}/_index.md`: change all of them (`grep -rn 399 website docs`).
Ordering is through Discord only (`buyUrl = ""`).

## Pinout drawings

The SVGs in `website/static/img/pinouts/` are generated; edit
`tools/pinouts/gen_svg.py`, never the SVGs, then run

```sh
python3 tools/pinouts/gen_svg.py
```

Every plug is drawn in the one orientation described in the script's
docstring (contacts facing the viewer, latch at the back, pin 1 at the
bottom). A new inverter needs its page in both languages with the same
`translationKey`, and a pinout only when it is confirmed.

## Preview and checks

```sh
tools/site/fetch-firmware.sh            # optional: release binaries for the install wizard (needs gh)
hugo server --source website            # http://localhost:1313/ (PL) and /en/
hugo --source website                   # the build CI runs
```

On a PR, CI uploads the built site as the `site-preview` artifact and, when
Cloudflare secrets are set, deploys it to the `pr-<number>` preview.

## After merge

- `pages.yaml` deploys the site from `main` (FAQ changes included).
- The support bot reads `docs/faq.md`, `docs/faq.pl.md`, `docs/pairing.md`
  and `docs/pairing.pl.md` from `origin/main` at its next hourly corpus
  build; site pages under `website/` are not in its corpus. In `../GbbSupportBot`, confirm with
  `uv run gbbbot -c config.toml search '<phrase>' --prefix gbbdongle-docs/`.
