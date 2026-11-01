"""Shared, validated identity input for an update image and its PPU1 package.

Physical/update-release builds provide ``PP_UPDATE_IDENTITY_FILE``.  Both the
pre-image ESP descriptor hook and the package builder read that one JSON file;
this makes a manifest/image identity disagreement a build-gate failure rather
than a field-test discovery.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path


CANONICAL_PRODUCT = "plonk-and-play"
CANONICAL_BOARD = "esp32-8048s070"


@dataclass(frozen=True)
class UpdateIdentity:
    product: str
    board: str
    build_id: str
    version: int

    def validate(self) -> "UpdateIdentity":
        if self.product != CANONICAL_PRODUCT:
            raise RuntimeError("update identity product must be plonk-and-play")
        if self.board != CANONICAL_BOARD:
            raise RuntimeError("update identity board must be esp32-8048s070")
        try:
            build = self.build_id.encode("ascii")
        except UnicodeEncodeError as exc:
            raise RuntimeError("update identity build_id must be ASCII") from exc
        if not build or len(build) > 31:
            raise RuntimeError("update identity build_id must be 1..31 ASCII bytes")
        if not isinstance(self.version, int) or not 1 <= self.version <= 0xFFFFFFFF:
            raise RuntimeError("update identity version must be a positive uint32")
        return self


def load_update_identity(path: str | Path) -> UpdateIdentity:
    source = Path(path)
    try:
        raw = json.loads(source.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"cannot read PP_UPDATE_IDENTITY_FILE: {source}") from exc
    if not isinstance(raw, dict):
        raise RuntimeError("update identity must be a JSON object")
    required = {"product", "board", "build_id", "version"}
    if set(raw) != required:
        raise RuntimeError("update identity must contain exactly product, board, build_id and version")
    return UpdateIdentity(**raw).validate()


def load_update_identity_from_environment() -> UpdateIdentity | None:
    configured = os.environ.get("PP_UPDATE_IDENTITY_FILE")
    if not configured:
        return None
    if os.environ.get("PP_APP_DESCRIPTOR_VERSION") or os.environ.get("PP_APP_DESCRIPTOR_SECURE_VERSION"):
        raise RuntimeError("legacy app-descriptor identity variables conflict with PP_UPDATE_IDENTITY_FILE")
    return load_update_identity(configured)
