# Stage 13 Design — Complete Lap Race

**Status: FROZEN before Stage 13 implementation**

Stage 13 completes the ordinary two-lane Lap Race boundary. It adds the authoritative start-light sequence, generic false-start reporting with Lap Race consequences, destructive post-pause abandonment operations, persistent completed-race History, and persistent Lap Race PB/Track Record foundations.

Existing Stage 1–12 authority, timing, Message Bus, Browser identity, settlement, integrity and result boundaries remain in force. Stage 13 introduces no supersession.

## Start sequence

The factory/default Lap Race start is five red lights, one second between successive red-light instants, a random final delay selected once in the range 0.5–2.0 seconds after the fifth red, and Lights Out as GO. Race Control commits the complete schedule and authoritative GO instant when START is accepted. Browser, audio and physical presentation consume that schedule and never determine GO.

The configured sequence supports three or five reds, Lights Out or Green GO, and Immediate, Fixed or Random final delay. Red-to-red interval is an Advanced setting. The selected random value is committed once per sequence. The implementation may use a deterministic seeded source in acceptance builds, provided product behaviour remains random within the configured range in normal operation.

The sequence completes and GO occurs even if every Browser disconnects or sleeps. A qualifying crossing with Relevant Time `t < GO` is a false start; `t >= GO` is legal, including exactly GO.

## False starts

False-start detection is offered only when the accepted Session Definition contains an installed/configured capability that can authoritatively detect the relevant competitor crossing before GO. The normal base/Taster sensors-before-Start/Finish arrangement does not claim this capability.

The generic event boundary identifies the affected Race Entry and authoritative Relevant Time. Input hardware and the Input Module do not apply race consequences. Lap Race applies one of:

- OFF — the false-start event may be recorded diagnostically, but produces no competitor warning, penalty or change to race outcome;
- WARNING — record and present the false start without changing required distance;
- +1 LAP — increase that entry's required genuine race distance by one lap.

Repeated +1 LAP false starts accumulate. Completed-lap counts are never falsified; a nominal ten-lap race with two penalties requires twelve genuine completed laps. GO is not aborted, rescheduled or restarted by a false start.

The generic event/consequence boundary remains suitable for later Endurance, Timed Stage and Drag policies, which are outside this stage.

## Pause abandonment operations

Restart Race and End Race are Race Director-only operations available only after PAUSE and the existing local pre-pause settlement fence have completed. They are not direct RACING controls.

Opening or cancelling a confirmation has no authoritative effect. Once confirmed and accepted:

- Restart Race abandons the interrupted race, creates no completed History entry, retains/seeds the same proposed setup, returns to READY, and causes the next START to create a fresh Session Definition and fresh Race Entry IDs with competition at zero.
- End Race abandons the interrupted race, creates no completed History entry, and returns to Home.

If a delayed eligible pre-pause event completes the race before acceptance, FINISHED wins and neither operation may destroy the completed race.

## Persistent History

Each settled completed race is stored automatically as a faithful persistent representation of the authoritative Stage 12 result. A persistent sequence identity orders records newest first and remains unique across power cycles, Race Again and rolling deletion. Civil time is optional metadata only when supplied by a trustworthy source; ordering never depends on an RTC.

History contains enough immutable Session Definition, result, ranking, W/F, finish-behaviour and lap-record data to reconstruct Stage 12 Results and Details. Abandoned races are absent. A substantial implementation-selected rolling capacity may evict the oldest record. Clear History is deliberate and separate from record clearing.

Persistent storage failure never changes, invalidates or recalculates authoritative timing or competition. The implementation surfaces a storage warning/fault and retains or retries recoverable pending persistence where possible.

Controller restart recovers persistent History and records but does not reconstruct an interrupted active race, PAUSED State, pending restart or volatile Session Definition.

## PBs and Track Records

The default standalone record context is record-eligible:

- Track: `DEFAULT TRACK`;
- competitors: `LANE 1` and `LANE 2`.

SMUG configuration may later replace these identities without Stage 13 implementing the complete Track/MUG configuration system.

A genuine valid completed timed lap may update the selected competitor PB and Track Record at lap completion, even if its containing race later ends through End Race. A lap is eligible when its own authoritative timing and validity are sound at the lap-completion boundary. A later race fault does not retrospectively invalidate an already-established eligible PB/Track Record unless that fault establishes that the lap itself was invalid. Honour-Restart timing is eligible; Grid-discarded incomplete progress and demo/acceptance simulated laps are not.

PB/record clearing supports individual MUG PB, Track Record, and all records for a Track. Each requires deliberate confirmation, preserves History, and starts a new persistent record era. Old History cannot resurrect a cleared record automatically. Clearing History does not clear PBs or records.

## Boundaries and deferrals

Stage 13 does not implement Practice, Endurance, Timed Stage, Drag, sectors, wireless/distributed input, Power Module behaviour, physical full RMS Yellow/Red enforcement, multi-lane expansion, car-specific records, physical-track equivalence detection, civil-time/RTC requirements, a full Track/MUG configuration system, a polished Browser redesign, Screen Designer, championship/event management, general multi-controller conflict resolution, future-mode penalty systems, active-race persistence/recovery, History browsing on the small Taster display, or automatic History rescanning after record-era clearing.

The Browser remains a presentation and request boundary. Race Control remains lifecycle/operation authority; Race Engine remains competition/result authority; Input Module remains role-neutral event and settlement authority; Facts remain events rather than a History database.
