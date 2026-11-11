#!/usr/bin/env bash
# Writes the ESP Web Tools install manifests and the OTA update manifests
# next to the release binaries.
#
#   tools/site/assemble-manifests.sh <firmware dir> <version> <release url>
#
# <firmware dir> must already hold every <config>[-ha].{factory,ota}.bin of
# the release. Devices in the field poll the update manifests by URL and
# resolve the .bin paths relative to them, so file names and the relative
# "path" values are stable API.
set -euo pipefail

DIR="$1"
VERSION="$2"
RELEASE_URL="$3"

# Board table: <config basename> <chipFamily> <manifest suffix> <display name...>
# Per-board manifests are required because chipFamily alone cannot
# distinguish the two plain-ESP32 boards (T-CAN485 and Kamami):
# each install button and each device's update entity points at its
# own manifest instead.
BOARDS='
gbbdongle              ESP32-S3 gbbdongle   Waveshare ESP32-S3-RS485-CAN
gbbdongle-tcan485      ESP32    tcan485     LilyGo T-CAN485
gbbdongle-kamami       ESP32    kamami      Kamami KAmod ESP32 ETH+PoE
gbbdongle-8di8do-wifi  ESP32-S3 8di8do-wifi Waveshare ESP32-S3-POE-ETH-8DI-8DO, WiFi
gbbdongle-8di8do-eth   ESP32-S3 8di8do-eth  Waveshare ESP32-S3-POE-ETH-8DI-8DO, Ethernet
'

# Each board gets four manifests: install + update for the factory
# line (legacy names, so devices in the field stay on it) and
# install + update for the HA-compatible line ("-ha" suffix).
while read -r config chip suffix name; do
  [ -z "$config" ] && continue

  for line in factory ha; do
    if [ "$line" = "factory" ]; then
      bin="$config"; msuffix="$suffix"; display="GbbDongle (${name})"
    else
      bin="${config}-ha"; msuffix="${suffix}-ha"; display="GbbDongle (${name}, Home Assistant)"
    fi
    md5=$(md5sum "$DIR/${bin}.ota.bin" | cut -d' ' -f1)

    # Per-board ESP Web Tools manifest (browser flashing over USB).
    cat > "$DIR/manifest-${msuffix}.json" <<EOF
{
  "name": "${display}",
  "version": "$VERSION",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "${chip}",
      "parts": [{ "path": "${bin}.factory.bin", "offset": 0 }]
    }
  ]
}
EOF

    # Per-board manifest for the esphome http_request update entity
    # (OTA); each variant yaml points update_manifest_url at its own.
    cat > "$DIR/update-manifest-${msuffix}.json" <<EOF
{
  "name": "${display}",
  "version": "$VERSION",
  "builds": [
    {
      "chipFamily": "${chip}",
      "ota": {
        "path": "${bin}.ota.bin",
        "md5": "${md5}",
        "summary": "GbbDongle $VERSION",
        "release_url": "${RELEASE_URL}"
      }
    }
  ]
}
EOF
  done
done <<< "$BOARDS"

# ── Legacy shared manifests ──
# Kept for devices already in the field (they poll this URL and
# migrate to their per-board manifest with their next OTA) and for
# old bookmarks. Deliberately NOT extended with the Kamami board
# (its chipFamily ESP32 would collide with the T-CAN485 entry) nor
# the 8DI8DO variants (ESP32-S3 would collide with the Waveshare
# RS485-CAN entry).
MD5_S3=$(md5sum "$DIR"/gbbdongle.ota.bin | cut -d' ' -f1)
MD5_TCAN485=$(md5sum "$DIR"/gbbdongle-tcan485.ota.bin | cut -d' ' -f1)
cat > "$DIR"/manifest.json <<EOF
{
  "name": "GbbDongle",
  "version": "$VERSION",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [{ "path": "gbbdongle.factory.bin", "offset": 0 }]
    },
    {
      "chipFamily": "ESP32",
      "parts": [{ "path": "gbbdongle-tcan485.factory.bin", "offset": 0 }]
    }
  ]
}
EOF

cat > "$DIR"/update-manifest.json <<EOF
{
  "name": "GbbDongle",
  "version": "$VERSION",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "ota": {
        "path": "gbbdongle.ota.bin",
        "md5": "$MD5_S3",
        "summary": "GbbDongle $VERSION",
        "release_url": "${RELEASE_URL}"
      }
    },
    {
      "chipFamily": "ESP32",
      "ota": {
        "path": "gbbdongle-tcan485.ota.bin",
        "md5": "$MD5_TCAN485",
        "summary": "GbbDongle $VERSION",
        "release_url": "${RELEASE_URL}"
      }
    }
  ]
}
EOF

jq . "$DIR"/manifest*.json "$DIR"/update-manifest*.json > /dev/null
