# Approved physical-validation artifact

Source implementation: `230f1ba09465f3751aa9916624b78848f2457925`  
Environment: `stage8bp1s3sd`  
Artifact: `firmware.bin`  
Size: `1,465,904` bytes  
Firmware SHA-256: `8387FDB7C85AED280E6CA9A18432B292226B12E7B6FA640F43DE2077FC69BFEC`  
Application ELF SHA-256: `D8EF4C54BE6D613A587FE86ACC79531D43947199F7C0ED7585E2EC2ADF2D9B13`

## Build

Built once from a clean detached worktree at the exact implementation commit.

- PlatformIO/espressif32: `8.0.0`
- Arduino-ESP32: `3.3.12`
- ESP-IDF: `5.5.5`
- Arduino_GFX: `1.6.5+sha.4c1d0a6`
- RAM: `88,884 / 327,680` bytes
- Flash: `1,437,619 / 6,553,600` bytes
- SSE workers: `4`
- SSE worker stack: `8,192` bytes each

## Artifact checks

- preserved-copy hash and size match `manifest.json`;
- generated Browser binary-content/provenance check: PASS;
- embedded source commit marker is `230f1ba09465f3751aa9916624b78848f2457925`;
- embedded environment marker is `stage8bp1s3sd`;
- prior `88515b88f4f972da52d3b6a89888483189ff1b3f` marker is absent;
- no rebuild or upload was performed after preservation.

The artifact is not flashed by this step.
