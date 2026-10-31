# REVOKED

These Known-Good A and Candidate B artifacts are revoked and must not be flashed.

Reason: the post-build app-identity mutation changed the descriptor after
`elf2image` without recomputing the ESP image checksum. The preserved images
are not valid boot images.

Use only the corrected artifacts under the later pre-image identity build.
