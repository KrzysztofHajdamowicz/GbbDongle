#!/usr/bin/env bash
# Local preview helper: downloads the .bin assets of a release (default:
# the latest) into website/static/firmware/ and writes the manifests next to
# them, so the install wizard works under `hugo server --source website`.
# The directory is gitignored; the Pages workflow does the same at deploy.
#
#   tools/site/fetch-firmware.sh [tag]
set -euo pipefail

cd "$(dirname "$0")/../.."
REPO=KrzysztofHajdamowicz/GbbDongle
TAG="${1:-$(gh release view --repo "$REPO" --json tagName -q .tagName)}"
DIR=website/static/firmware

rm -rf "$DIR"
mkdir -p "$DIR"
gh release download "$TAG" --repo "$REPO" --pattern '*.bin' --dir "$DIR"
tools/site/assemble-manifests.sh "$DIR" "${TAG#v}" "https://github.com/${REPO}/releases/tag/${TAG}"
echo "Firmware $TAG in $DIR"
