# Study 09 — the main study, experiment E0

The stacks, fires and groups of pictures (GOP) below cross into 36 sets of
20 simulations, 720 in all, at an alarm period of 9 s on the
10 × 10 grid of 40 m. `sets.txt` lists the sets by the names
`../../sets.conf` declares.

| Token | Values |
|---|---|
| stack | `set_AOMDV_…-pq`: AOMDV on the release's stack; `set_AOMDV_QPV_…-rules`: the cross-layer rules with the early discard; `-agents`: the seven learning agents; `-hybrid`: six agents and the three-failure rule of the link-break detection |
| fire | `-fire` (F1, the middle of the camera-sink axis), `-fire2` (F2, the camera side), `-fire3` (F3, the sink side) |
| GOP | `GC-PN-0p01597-13` (3-9), `GC-PN-0p02525-13` (6-6), `GC-PN-0p03-13` (9-3): the GOP coefficient of the encoder file `../../ipFiles/<label>.dat`, thirteen priority levels |

**Seeds.** Every set of a fire takes the seeds of
`set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-<fire>` (`run_sets.sh --seed-name`):
the stacks and the GOPs of a fire undergo the same failures, realisation by
realisation, and every comparison is paired. The seed of realisation k is
`cksum("<name>-<k>") mod 900000 + 1` (`../../run/run_sets.sh`, `seed_for`).

**Results.** `results/runs.tsv` holds one row per simulation, with its seed
(`../../metrics/compile_runs.py`); `results/sets.tsv` one row per set, the
mean, standard deviation, minimum and maximum over its realisations of each
measure of `sim/mt.dat`, and the aggregates of `../../metrics/setGraphs.pl`
for the others (`../../metrics/compile_sets.py`); `results/routing.tsv` the
routing counters of the AOMDV and rule stacks at GOP 9-3
(`../../metrics/routing_events.py`).
