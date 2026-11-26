---
name: release
description: Use when preparing a GbbDongle firmware release (a v* tag), when an ESPHome or toolchain bump has landed on main, or when a release or its Pages deploy needs checking or recovering.
---

# Preparing a release

Tagging `v*` is the only step that ships firmware: every published device
polls its update manifest every 6 h and installs a new release within a
random 0–5 h. **The agent prepares and checks; Krzysztof creates the tag.**
Never tag, re-tag or delete a tag yourself.

## 1. What is in it

```sh
git fetch --tags origin
git describe --tags --abbrev=0 origin/main          # last release
git log --oneline <last-tag>..origin/main
git diff --stat <last-tag>..origin/main -- components firmware pyproject.toml uv.lock
```

Summarize the changes for Krzysztof in user terms (what a device owner will
notice). Dependabot squash-merges ESPHome and toolchain bumps on its own once
CI is green, so a release often carries an ESPHome version nobody has run.

## 2. Is it safe to ship

- CI is green on `origin/main` (`gh run list --branch main --limit 5`).
  Green only means **everything compiles**.
- If anything under `components/`, `firmware/`, or the esphome pin changed
  since the last tag, a bench check on hardware is required before tagging.
  Krzysztof runs it, or you run it with him at the bench
  (`tools/bench/README.md`):
  1. MQTT over TLS to a broker (the published images always use TLS);
  2. a Modbus round trip through `tools/cloud_roundtrip.py` and the bench
     slave;
  3. a live RS485 baud/parity change;
  4. Factory line: Admin Password rotation (web and OTA) and an empty value
     restoring the default;
  5. an update check against a manifest;
  6. pairing with a GbbOptimizer code, if `cloud_pairing` changed.
- Report what was and was not verified. Don't call a release ready without
  item 1 and 2.

## 3. After Krzysztof tags

1. `release.yaml`: compile matrix with `-s version <tag without v>`, SBOM and
   attestations, GitHub release with notes, then it calls `pages.yaml`.
2. `pages.yaml` deploys the site plus binaries and manifests under a
   per-deployment commit object, then fails unless the live
   `firmware/update-manifest.json` reports the new version.
3. Check by hand:
   `curl -s https://gbbdongle.hajdamowicz.info/firmware/update-manifest.json | jq .`
   and one per-board manifest.

If the Pages job failed after a successful release, read its log first. The
deploy can be repeated with
`gh workflow run pages.yaml -f tag=<tag>` — that publishes to every device
in the field, so ask Krzysztof before running it.

## 4. Downstream

- The support bot's corpus (`../GbbSupportBot`, source `latest-tag:v*`)
  switches to the new tag at its next hourly build.
- User-visible changes (new setting, changed default, new board) need
  `docs/faq.md`/`docs/faq.pl.md` or site updates: see the `user-docs` skill.
