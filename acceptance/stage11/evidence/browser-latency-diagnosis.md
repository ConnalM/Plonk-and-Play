# Stage 11 Browser latency diagnosis

Date: 2026-10-02

## Observed path

The Browser sends control POSTs directly to the Browser Interface route. Race Control receives the resulting `SessionOperationRequest` through the normal Message Bus and assigns the authoritative pause instant when it accepts the operation. Noticeboard State is published independently of Browser rendering. Browser rendering is a polling concern and is not used to determine `P` or to gate race processing.

## Measurements

Repeated read-only HTTP measurements against `http://127.0.0.1:9080` (8 samples each) were sub-second:

| endpoint | min | median | max |
|---|---:|---:|---:|
| `/` | 0.233 s | 0.271 s | 0.334 s |
| `/noticeboard` | 0.163 s | 0.240 s | 0.274 s |
| `/state` | 0.181 s | 0.192 s | 0.225 s |
| `/context` | 0.178 s | 0.190 s | 0.208 s |
| `/fact` | 0.179 s | 0.182 s | 0.197 s |

These timings do not show a five-second gateway or ESP32 HTTP control path.

## Cause and correction

The diagnostic page scheduled a new 250 ms polling pass without an in-flight guard. Each pass performed several sequential HTTP requests, including an unconditional `/fact` request. Under a slow private gateway or presentation pressure, overlapping passes could exhaust the small ESP32 HTTP socket budget and delay visible updates even though the authoritative operation had already been accepted.

The Browser page now serialises polling with an in-flight guard and fetches `/fact` only when Noticeboard revision changes. This reduces concurrent HTTP pressure while preserving current-State reconstruction. The control request remains independent of polling, and Race Control's authoritative timing does not depend on the Browser refresh completing.

## Verification

- Stage 11 acceptance Wokwi scenario: `ACC DONE`; tests 11.1–11.20 and 11.T evidence lines present.
- Stage 11, 10 and 9 structural reviews: PASS.
- Stage 11 acceptance and demo firmware builds: PASS.
- Human checkpoint remains paused and is not marked PASS by this evidence.
