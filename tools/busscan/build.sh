#!/bin/sh
# Cross-compile busscan for the platforms we hand to testers.
set -eu
cd "$(dirname "$0")"

mkdir -p dist
for t in windows/amd64 linux/amd64 linux/arm64 darwin/arm64 darwin/amd64; do
  os=${t%/*}
  arch=${t#*/}
  ext=
  [ "$os" = windows ] && ext=.exe
  echo "building dist/busscan-$os-$arch$ext"
  GOOS=$os GOARCH=$arch CGO_ENABLED=0 \
    go build -trimpath -ldflags="-s -w" -o "dist/busscan-$os-$arch$ext" .
done
# macOS ships a fat-binary format; merge both darwin slices into one file
# so the tester doesn't have to know their Mac's CPU. lipo comes with the
# Xcode Command Line Tools, so skip quietly elsewhere.
if command -v lipo >/dev/null 2>&1; then
  echo "building dist/busscan-darwin-universal"
  lipo -create dist/busscan-darwin-amd64 dist/busscan-darwin-arm64 \
    -output dist/busscan-darwin-universal
fi

echo "done:"
ls -lh dist/
