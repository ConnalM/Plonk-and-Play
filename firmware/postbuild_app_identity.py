"""Patch the ESP app descriptor product name for normal P&P builds.

Arduino-ESP32's prebuilt app descriptor currently carries the lib-builder
project name.  The controller's signed-update boundary uses the canonical
P&P product identity, so the normal PlatformIO output is corrected at the
binary metadata boundary and its ESP image validation hash is recomputed.
"""

from pathlib import Path
import hashlib
import struct

Import("env")

CANONICAL_PRODUCT = b"plonk-and-play"
APP_DESC_OFFSET = 0x20
PROJECT_NAME_OFFSET = APP_DESC_OFFSET + 0x30
VALIDATION_HASH_BYTES = 32


def patch_app_identity(source, target, env):
    del source, env
    image = Path(str(target[0]))
    data = bytearray(image.read_bytes())
    if len(data) <= PROJECT_NAME_OFFSET + 32 + VALIDATION_HASH_BYTES:
        raise RuntimeError("ESP app image is too small for the app descriptor")
    magic = struct.unpack_from("<I", data, APP_DESC_OFFSET)[0]
    if magic != 0xABCD5432:
        raise RuntimeError("ESP app descriptor magic is missing")
    name = CANONICAL_PRODUCT + b"\0" * (32 - len(CANONICAL_PRODUCT))
    data[PROJECT_NAME_OFFSET : PROJECT_NAME_OFFSET + 32] = name
    data[-VALIDATION_HASH_BYTES:] = hashlib.sha256(data[:-VALIDATION_HASH_BYTES]).digest()
    image.write_bytes(data)
    observed = bytes(data[PROJECT_NAME_OFFSET : PROJECT_NAME_OFFSET + 32]).split(b"\0", 1)[0]
    if observed != CANONICAL_PRODUCT:
        raise RuntimeError("canonical ESP app product identity was not written")
    print("[BUILD] app_descriptor_product=plonk-and-play")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", patch_app_identity)
