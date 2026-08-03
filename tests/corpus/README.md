# Trace corpus (A3)

Committed per scenario:

| File | Role |
|---|---|
| `input.atin` | Input log (source of truth for driving the engine; v2 carries the scenario setup id) |
| `engine.attr` | Our engine under that log |
| `reference.attr` | Oracle ATTR — Wave-1 **stub** (`ball.x + 1` raw/tick) until an instrumented `SWOS_TEST` Amiga recording replaces it |
| `*.chain` | FNV hash chain over per-record payload hashes |

Regenerate: `cmake --build --target gen-corpus`

Viewer demo:

```text
trace_viewer tests/corpus/kickoff/engine.attr tests/corpus/kickoff/reference.attr
```

Expect first divergence at tick 1 (stub engine vs stub oracle).

## Scenario setup ids

A scenario may name a starting state (`tracekit::ScenarioSetup`) applied after
the bootstrap tick. The id is serialised into the `.atin`, so the committed log
plus the enum still reproduce the trace. `shot_curl` uses one: it previously
scripted fire+NE while every home player was ~200 units from the ball, and so
recorded a walk rather than a kick (B6a).
