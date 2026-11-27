# WP8 Core build preparation

The recovered Arduino component is untracked and has no Git metadata. Its
CMake manifest SHA-256 is
`846B520B224EB4EDE376C399DBFF533F840A6575E46EACE9EB2CC0B88FCF67D6`.
Its recovered sdkconfig disables selective Arduino compilation.

Before a Core configure gate, capture a complete component content manifest,
version identity and generated dependency graph. Do not download, replace or
modify the recovered dependency.

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
