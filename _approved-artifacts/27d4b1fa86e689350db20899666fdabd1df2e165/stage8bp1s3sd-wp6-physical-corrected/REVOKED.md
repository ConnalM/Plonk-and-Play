# Revoked WP6 physical-update artifacts

This A/B artifact set is revoked for physical update use.  Its signed PPU1
manifests used the intended identities, but its final ESP images did not:

- Known-Good A manifest: `known-good-A` / version `100`;
- Candidate B manifest: `candidate-B` / version `101`;
- image descriptors: the old build identifiers and secure version `1`.

The production adapter correctly rejected those mismatches.  Do not flash or
upload these packages.  They are superseded by the artifact set produced from
the single controlled identity input introduced after this finding.
