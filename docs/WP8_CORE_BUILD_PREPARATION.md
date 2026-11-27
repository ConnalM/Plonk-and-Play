# WP8 Core build preparation

The recovered Arduino component is untracked and has no Git metadata. Its
CMake manifest SHA-256 is
`846B520B224EB4EDE376C399DBFF533F840A6575E46EACE9EB2CC0B88FCF67D6`.
Its recovered sdkconfig disables selective Arduino compilation.

The recovered component is pinned as follows (read-only inventory on
2026-11-27):

- location: `firmware/wp7_idf_arduino_dbuf/components/arduino`;
- package version: `4.30312.261024`;
- component identity: Espressif Arduino core, `arduino-esp32`;
- files: `2,175`;
- content-manifest SHA-256:
  `9436B05E5B04B09BE5315C2CA44AE1DAA31F04D8648B370C1BEA4B975081F129`.

The manifest hash is the SHA-256 of sorted UTF-8 lines
`relative-path<TAB>byte-length<TAB>file-SHA-256`, each terminated by LF.
Before a Core configure gate, regenerate that complete manifest and compare
both hashes and the file count before and after the disposable build. Do not
download, replace or modify the recovered dependency.

The proposed independent component selection is `firmware/core` with
`core_runtime_esp32.cpp` and the existing, single shared
`firmware/src/system_time.cpp`; the inactive role profile records the same
paths relative to `firmware`. The integrated `main` component is deliberately
not selected. Source provenance is the reviewed Git commit plus the recovered
Arduino content manifest; the result remains unproven until the later
configure gate produces the generated component graph.

Use a disposable short workspace such as `C:\\ppcore\\<source-sha>` with
process-local paths to pinned recovered inputs, a copied disposable SDK config
and a new build directory. Do not modify bwp7welcome, accepted configuration
or global drive mappings. Configure with
`-DCMAKE_NINJA_FORCE_RESPONSE_FILE=ON`, retain the command, cache,
compile_commands.json, Ninja rules and response files, and prove Core-owned
translation units receive the Core marker and forced guard with no fixture,
simulation, test-key or Wi-Fi scan instrumentation flag.

Stop on a command-length or identity discrepancy. Compilation/link, image and
flash each need separate approval.
