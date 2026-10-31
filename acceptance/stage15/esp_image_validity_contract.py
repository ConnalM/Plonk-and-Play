"""Validate the linked app identity and final ESP image integrity."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from elftools.elf.elffile import ELFFile


CANONICAL_PRODUCT = b"plonk-and-play"
APP_DESC_MAGIC = 0xABCD5432
APP_DESC_SECTION = ".flash.appdesc"
PROJECT_NAME_OFFSET = 0x30


def check_elf(path: Path) -> None:
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        section = elf.get_section_by_name(APP_DESC_SECTION)
        if section is None or section["sh_size"] < 256:
            raise AssertionError("ELF lacks a complete .flash.appdesc section")
        data = section.data()
    magic = int.from_bytes(data[:4], "little")
    if magic != APP_DESC_MAGIC:
        raise AssertionError(f"ELF app descriptor magic is 0x{magic:08x}")
    name = data[PROJECT_NAME_OFFSET : PROJECT_NAME_OFFSET + 32].split(b"\0", 1)[0]
    if name != CANONICAL_PRODUCT:
        raise AssertionError(f"ELF project name is {name!r}")
    print("ELF project name: plonk-and-play (PASS)")


def esptool_path(explicit: str | None) -> str:
    if explicit:
        return explicit
    for candidate in ("esptool", "esptool.exe"):
        found = shutil.which(candidate)
        if found:
            return found
    user_profile = os.environ.get("USERPROFILE")
    if user_profile:
        candidate = Path(user_profile) / ".platformio" / "packages" / "tool-esptool" / "esptool.exe"
        if candidate.exists():
            return str(candidate)
    raise AssertionError("esptool executable not found")


def check_image(path: Path, tool: str | None) -> None:
    command = [esptool_path(tool), "--chip", "esp32s3", "image-info", str(path)]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    output = result.stdout + result.stderr
    if result.returncode != 0:
        raise AssertionError(f"esptool image-info failed ({result.returncode})\n{output}")
    if not re.search(r"Checksum:\s+0x[0-9a-f]+\s+\(valid\)", output, re.IGNORECASE):
        raise AssertionError("final image ESP checksum is not reported valid")
    if not re.search(r"Validation hash:.*\(valid\)", output, re.IGNORECASE):
        raise AssertionError("final image validation hash is not reported valid")
    if not re.search(r"Project name:\s+plonk-and-play\s*$", output, re.IGNORECASE | re.MULTILINE):
        raise AssertionError("final image project name is not canonical")
    print("Final image project name: plonk-and-play (PASS)")
    print("Final image checksum: valid (PASS)")
    print("Final image validation hash: valid (PASS)")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--esptool")
    args = parser.parse_args()
    check_elf(args.elf)
    check_image(args.image, args.esptool)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
