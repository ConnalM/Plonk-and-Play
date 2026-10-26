# History decoder physical validation — 54e6584

Date: 2026-10-26

## Candidate

- Source commit: `54e6584d39b9b88a056d15294c1d51160012ae0e`
- Environment: `stage8bp1s3sd`
- Firmware: `firmware/.pio/build/stage8bp1s3sd/firmware.bin`
- Size: `1,454,400` bytes
- SHA-256: `491D20F16544F148D83D27A3B184C41AA81B88054159F5CB15B3CFEFE706DFBC`

## Boot and read-only request

Fresh COM7 capture: `firmware/logs/device-monitor-261026-162836.log`.

The running controller reported:

```text
[P8-P1] SD owner mount OK
[P8-P1] History store begin owner_lease=1 owner_mounted=1 owner_health=1
[P8-P1] SD mount OK card=15931539456 total=15926558720 used=22282240
[BUILD] source_commit=54e6584d39b9b88a056d15294c1d51160012ae0e environment=stage8bp1s3sd app_elf_sha256=8130be81a
[INIT] Phase 8B-P1 SD owner READY; History storage boundary READY (namespace reconstruction lazy); NVS configuration READY=YES
[DEV] Browser HTTP server STARTED error=0
[WIFI] STA_EVENT event=GOT_IP ip=192.168.68.102
```

Exactly one read-only `GET /history` request was issued over LAN. The HTTP response completed successfully and returned sequence 9 with:

- `sealed=true`
- `mode=LAP_RACE`
- `finishBehaviour=COMPLETE_FULL_RACE_DISTANCE`
- two entries, both completed at two laps;
- stored detail represented by the reconstructed result.

The response also returned the older History sequences. No History decode fault, SD write/repair/migration, panic, or reboot appeared in the complete capture after the request.

## Result

**PASS — existing sequence-9 History payload decodes and is served correctly on hardware without modifying SD contents.**

