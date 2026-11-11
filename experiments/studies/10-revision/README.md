# Study 10 — the other experiments of the revised article

Study 09 is experiment E0 of the article's plan; this study holds the sets of
the others, E1 to E5, and of the agents' traces. Every set takes the seeds of
study 09
(`run_sets.sh --seed-name=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-<fire>`),
20 realisations per set: the 60 seeds (fire, realisation) each give
one failure schedule, identical across stacks and GOPs, so that every
comparison with study 09 is paired. Set names end in `-s10` (`-frozen` for the
frozen policy), so that no set of study 09 is overwritten. `sets.txt` lists the
261 sets, 5220 simulations; the knobs behind each suffix are in the last
column of `../../sets.conf`, and the simulator's switches in
`../../../ns-2-modules/README.md`.

| Experiment | Series | Suffix | Stacks | GOP | Sets (simulations) | What it controls |
|---|---|---|---|---|---|---|
| E1 | control AOMDV | `-pq-ctl-s10` | AOMDV | 3-9, 6-6, 9-3 | 9 (180) | classic AOMDV on the derived PHY and MAC, transmission watchdog included (`aomdv_derived=1`); a control, never the reference |
| E1 | AOMDV with cross-layer timers | `-pq-tuned-s10` | AOMDV | 3-9, 6-6, 9-3 | 9 (180) | the control AOMDV with the cross-layer stacks' routing timers and route queue (`aomdv_tuned=1`); a control, never the reference |
| E2 | one agent alone | `-only<X>-s10` | rules | 9-3 | 21 (420) | one agent alone: agent X in place of its rule on the rule stack |
| E2 | one agent out | `-no<X>-s10` | hybrid | 9-3 | 18 (360) | one agent out: agent X taken out of the hybrid stack |
| E3 | frozen | `-frozen` | hybrid, agents | 3-9, 6-6, 9-3 | 18 (360) | every learning and exploration rate at 0: each agent stays on its initial choice |
| E3 | decaying ε | `-edec-s10` | hybrid, agents | 9-3 | 6 (120) | exploration falling linearly to 0 between 60 and 114 s |
| E4 | equal rate | `_p9r65`, `_p9r32` | rules, hybrid | 3-9, 6-6 | 12 (240) | GOPs 3-9 and 6-6 at the mean packet rate of GOP 9-3 (T = 9.65 and 9.32 s) |
| E4 | coefficient 0.1 | `_GC-QC-0p1_`, `-t168-s10` | all four | 9-3 | 12 (240) | the quality coefficient 0.1 instead of 0.01 (`../../ipFiles/GC-QC-0p1.dat`), simulated for the 168 s of their study 09 counterparts (`simtime=168`) |
| E5 | load | `_p7`, `_p12`, `_p18`, `_p30` | all four | 9-3 | 48 (960) | alarm periods of 7, 12, 18 and 30 s instead of 9 s |
| E5 | fire | `-az015`, `-az05`, `-az165`, `-pf0`, `-pf025`, `-pf075`, `-fd1` | rules, hybrid | 9-3 | 42 (840) | heated-zone attenuation of 0.15, 0.5 and 1.65 dB/m instead of 1; failure probability of 0, 0.25 and 0.75 instead of 0.5; failures closer to the front |
| E5 | scenario | `-forest`, `-smoke`, `-j8`, `-swap`, `-v15` | all four | 9-3 | 60 (1200) | two other videos, at their own GOP 9-3 coefficients; a 30 m grid with an irregular deployment of ±8 m and scaled ignitions; camera and sink swapped; head speed times 1.5 |
| the agents' traces |  | `-tr-s10` | hybrid, agents | 9-3 | 6 (120) | `rl_trace=1`: each agent's action counts and values every 20 s; the runs replay those of study 09 exactly |

The agents' letters: P (path), C (congestion threshold), B (link-break
detection), D (discovery), A (channel access), T (transmit power), E (early
discard). The knob `simtime=S` sets a floor of S seconds on the simulated time;
`../../run/set.sh` reads it and never passes it to ns-2, so that a set without
it simulates up to its last packet plus 3 s, as before.

The sets were run in tranches, which some tools name: T0, the frozen policy;
T1, the traces, the control AOMDV and the decaying exploration; T2, the agents
one by one; T3, the load and the equal rate; T4, the fire; T5, the scenario;
T8, the tuned AOMDV; T9, the quality coefficient 0.1 at 165 s; T10, the same
at 168 s. T6 and T7 were dropped before they ran.

**Results.** `results/runs.tsv` holds one row per simulation, with its seed
(`../../metrics/compile_runs.py`). The routing counters come from each set's
`events.tsv` (`../../metrics/routing_events.py`), written before the packet
traces were deleted.
