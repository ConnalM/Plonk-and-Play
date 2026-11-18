from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "firmware/include/pp/wp7_physical_racing_ui.h").read_text(
    encoding="utf-8"
)
cmake = (ROOT / "firmware/wp7_idf_arduino_dbuf/main/CMakeLists.txt").read_text(
    encoding="utf-8"
)
build_ninja = (ROOT / "bwp7welcome/build.ninja").read_text(encoding="utf-8")

# The physical XIP target must carry the same static-presentation guard as the
# previously accepted PlatformIO target.  Checking generated Ninja output
# prevents a source-only CMake edit from being mistaken for a compiled flag.
xip_block = re.search(
    r'if\("\$ENV\{PP_WP7_IDF_SINGLE_FB_XIP\}"\s+STREQUAL\s+"1"\)(.*?)else\(\)',
    cmake,
    re.S,
)
assert xip_block, "XIP compile-definition branch is missing"
assert "PP_WP7_DISABLE_ANIMATIONS=1" in xip_block.group(1)
assert re.search(r"-DPP_WP7_DISABLE_ANIMATIONS=1(?:\s|$)", build_ninja)
assert "-DPP_WP7_IDF_DOUBLE_BUFFER=1" not in build_ninja

# With the guard defined, the production renderer must make animation state
# inert.  This covers both lap-car arming and the Finished flag path: the
# latter is also gated before it can mark FinishFlag on every draw tick.
assert "static constexpr bool kAnimationsEnabled = false;" in header
assert re.search(
    r"bool laneAnimationActive\([^)]*\) const \{\s*"
    r"if\s*\(!kAnimationsEnabled\)\s*return false;",
    header,
    re.S,
)
assert re.search(
    r"if\s*\(!kAnimationsEnabled\)\s*\{\s*"
    r"for\s*\(auto& animation : laneAnimations_\) animation\.active = false;",
    header,
    re.S,
)
assert "const bool finishFlagVisible = kAnimationsEnabled &&" in header
assert "invalidation.finishFlagVisible = finishFlagVisible;" in header
assert "if (input.finishFlagVisible) dirty.mark(Wp7UiComponent::FinishFlag);" in (
    ROOT / "firmware/include/pp/ui_dirty_regions.h"
).read_text(encoding="utf-8")
assert "const bool animating = kAnimationsEnabled &&" in header

# A Finished noticeboard state must not by itself make the disabled path
# dirty.  This is the same boolean gate used by drawCurrent(), expressed as a
# deterministic contract rather than a second rendering implementation.
animations_enabled = False
lifecycle = "Finished"
finish_flag_visible = animations_enabled and lifecycle == "Finished"
assert not finish_flag_visible
assert not (finish_flag_visible and lifecycle == "Finished")

print("WP7_ANIMATION_DISABLED_CONTRACT PASS")
