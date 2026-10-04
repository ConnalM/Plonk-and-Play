from pathlib import Path
import argparse
import json
import random


def schedule(rng, red_count, interval, mode):
    if mode == "IMMEDIATE":
        delay = 0
    elif mode == "FIXED":
        delay = 750_000
    else:
        delay = rng.randint(500_000, 2_000_000)
    return 1_000_000 + red_count * interval + delay, delay


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=13013)
    parser.add_argument("--runs", type=int, default=200)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).parent / "evidence" / "stress-scenarios.jsonl")
    args = parser.parse_args()
    rng = random.Random(args.seed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    history, lane_pb = [], [0, 0]
    next_sequence, era = 0, 1

    with args.output.open("w", encoding="utf-8") as evidence:
        evidence.write(json.dumps({"kind": "stage13-seeded-stress", "seed": args.seed,
                                   "runs": args.runs}, sort_keys=True) + "\n")
        for run in range(args.runs):
            reds = rng.choice((3, 5))
            interval = rng.choice((250_000, 1_000_000))
            mode = rng.choice(("IMMEDIATE", "FIXED", "RANDOM"))
            go, delay = schedule(rng, reds, interval, mode)
            # Relevant Time, not the deliberately shuffled delivery order,
            # establishes the false-start boundary.
            delivered = [(go + rng.randint(1, 80_000), "legal"), (go - 1, "false")]
            rng.shuffle(delivered)
            false_start = [time for time, kind in delivered if kind == "false"]
            legal = [time for time, kind in delivered if kind == "legal"]
            assert false_start == [go - 1] and legal[0] >= go

            policy = rng.choice(("OFF", "WARNING", "+1 LAP"))
            target = rng.randint(1, 4)
            genuine_laps = target + (1 if policy == "+1 LAP" else 0)
            lap_times = [rng.randint(1_000_000, 12_000_000) for _ in range(genuine_laps)]
            finish = go + sum(lap_times)
            assert genuine_laps == target + (1 if policy == "+1 LAP" else 0)

            for lane in range(2):
                sample = lap_times[min(lane, len(lap_times) - 1)]
                if not lane_pb[lane] or sample < lane_pb[lane]:
                    lane_pb[lane] = sample

            next_sequence += 1
            history.append({"sequence": next_sequence, "finish": finish,
                            "laps": genuine_laps, "records": tuple(lap_times)})
            if len(history) > 4:
                history.pop(0)
            assert [entry["sequence"] for entry in history] == sorted(
                entry["sequence"] for entry in history)
            assert len({entry["sequence"] for entry in history}) == len(history)

            if run and run % 37 == 0:
                # A new record era does not alter or resurrect retained History.
                old_history = tuple(history)
                lane_pb = [0, 0]
                era += 1
                assert tuple(history) == old_history

            evidence.write(json.dumps({
                "run": run, "redCount": reds, "intervalUs": interval,
                "mode": mode, "finalDelayUs": delay, "go": go,
                "deliveryOrder": delivered, "falseStartAt": false_start[0],
                "policy": policy, "target": target, "genuineLaps": genuine_laps,
                "finish": finish, "historyNewest": history[-1]["sequence"],
                "recordEra": era, "lanePb": lane_pb,
            }, sort_keys=True) + "\n")

    print(f"Stage 13 deterministic stress PASS seed={args.seed} runs={args.runs}")


if __name__ == "__main__":
    main()
