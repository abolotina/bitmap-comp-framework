#!/usr/bin/env bash
# Remove generated artifacts: build/, queries/, results/, and generated
# datasets. The committed example dataset (data/example) is kept.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for d in build queries results; do
  if [ -e "$ROOT/$d" ]; then
    rm -rf "${ROOT:?}/$d"
    echo "removed $d/"
  fi
done

if [ -d "$ROOT/data" ]; then
  for d in "$ROOT/data"/*/; do
    [ -d "$d" ] || continue
    if [ "$(basename "$d")" != "example" ]; then
      rm -rf "$d"
      echo "removed data/$(basename "$d")/"
    fi
  done
fi

echo "done"
