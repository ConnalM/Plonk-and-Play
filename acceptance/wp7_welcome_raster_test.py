#!/usr/bin/env python3
"""Offline contract test for the approved WP7 welcome raster."""

from __future__ import annotations

import os
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "firmware" / "tools"))

from generate_wp7_welcome_raster import check  # noqa: E402


source = Path(
    os.environ.get(
        "PP_WP7_WELCOME_SOURCE",
        str(ROOT / "firmware" / "assets" / "wp7_welcome_static_flat_800x480.png"),
    )
)
asset = ROOT / "firmware" / "assets" / "wp7_welcome_static_rgb565.bin"
preview = ROOT / "firmware" / "assets" / "wp7_welcome_static_rgb565_roundtrip.png"

result = check(source, asset, preview)
assert result["raster_bytes"] == 768000
assert result["sentinels"]["white"]["rgb565"] == "0xFFFF"
assert result["sentinels"]["yellow"]["rgb565"] == "0xFE60"
assert result["sentinels"]["navy_top_left"]["rgb565"] == "0x0063"
assert result["high_byte_nonzero_words"] > 0
print("WP7_WELCOME_RASTER PASS")
print(result)
