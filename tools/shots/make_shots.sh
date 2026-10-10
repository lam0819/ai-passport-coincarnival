#!/usr/bin/env bash
# Host harness: runs the firmware's own field / machine / art / gfx code on
# the PC. Checks the controls, the coin ledger, the long-run payout (~50 %),
# the bonuses, the never-stuck refills, saving, and the coin pool fallback;
# estimates the per-frame cost; renders screenshots into docs/.
# Needs only cc and python3. Fails if a check fails.
set -euo pipefail
cd "$(dirname "$0")/../.."
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cc -std=c11 -O2 -D_POSIX_C_SOURCE=199309L -DGFX_COUNT_PIXELS -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -Itools/shots/stub -Imain \
    tools/shots/shots.c main/field.c main/machine.c main/art.c main/gfx.c main/assets.c -lm -o "$TMP/shots"
"$TMP/shots" "$TMP"
python3 tools/shots/to_png.py "$TMP" docs
