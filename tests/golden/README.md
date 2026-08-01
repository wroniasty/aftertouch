# Golden traces

Committed engine traces for A6 regression. Regenerated, never hand-edited.

| File | Scenario | Regen |
|---|---|---|
| `kickoff.attr` | seed `0xA5A50001`, 100 ticks (`tracekit::KickoffScenario`) | `cmake --build --target gen-golden` |

A failing `core_tests` golden case means `MatchEngine::Step` (or the scenario) changed.
If the change was intentional, regen and commit the new file with a note in the commit
message. If it was not, you have a regression.
