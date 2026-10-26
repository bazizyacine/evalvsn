# EvalVSN — reinforcement learning agents in a cross-layer stack for video over wireless sensor networks

Supporting material for the article *Reinforcement Learning Agents in a
Cross-Layer Stack for Forest Fire Video Surveillance over Wireless Sensor
Networks* (M. Y. Baziz, L. Sekhri and B. Kechar), submitted to *Sensors*
(MDPI).

| Directory | Contents |
|---|---|
| `evalvsn/` | the EvalVSN video codec and evaluation chain (C++17, OpenCV 4.6), with its `CMakeLists.txt` |
| `ns-2-modules/` | every file added to or modified in ns-2.35, at its path under `ns-2.35/`: the cross-layer routing agent `aomdv_qpv/` (AOMDV Quality Prioritized Video, AOMDV_QPV) with its learning agents, the derived 802.15.4 MAC and PHY, the power monitor and the fire propagation model under `mac/`, the priority interface queue under `queue/`, the video source and sink under `evalvsn/`, and the release files they touch; its `README.md` lists them |
| `experiments/` | the simulation chain: `run/` drives the simulations, `metrics/` extracts the measurements and holds the connectivity oracle, `ipFiles/` holds the encoder parameters, `sets.conf` declares every set and `studies/09-comparative/sets.txt` lists the 36 sets of the article |
| `article/tools/` | the compilation of the 720 simulations, the paired tests with Holm correction, the analysis of variance, and the tables and figures of the article |
| `article/src/data/` | the measurements behind every number of the article: `comparative.tsv`, one row per simulation, the fire maps and the decoded frames of the gallery |

## Building

1. ns-2.35 from `ns-allinone-2.35`: copy `ns-2-modules/` over `ns-2.35/`,
   then build as usual (`./configure && make` in `ns-2.35/`). The release's
   802.15.4 classes are left untouched; the stack lives in derived classes.
2. The codec: `cmake . && make` under `evalvsn/`, which builds `bin/evalvsn`.
3. The video: the fire sequence comes from the FLAME 2 dataset (Chen et al.,
   IEEE DataPort, 2022), converted to grayscale QCIF (176 × 144 pixels,
   twelve frames); it is not redistributed here.

## Configuration of the study

As in Table 3 of the article.

| | Parameter | Value |
|---|---|---|
| Common configurations | Simulator | ns-2.35, IEEE 802.15.4 at 2.4 GHz, 250 kbit/s |
| | Topology | grid of 10 × 10 nodes spaced 40 m apart, camera and sink at opposite corners |
| | Radio | AT86RF231, transmission at +3 dBm, sensitivity of −101 dBm |
| | Clear channel assessment (CCA) | threshold of −91 dBm |
| | Propagation | log-distance with exponent 3.2, range of 48.6 m |
| | Interface queue | 16 packets |
| | Video | FLAME 2, 176 × 144 pixels in grayscale, 12 frames, 13 priority levels |
| | Traffic | one 32-byte reading every 60 s, then from detection, at 60 s, one image every 9 s, in packets of at most 74 bytes sent every 15 ms |
| | Simulated duration | 168 s |
| | Agents | learning rate of 0.3 (routing) and 0.2 (access), exploration of 0.1 |
| Studied factors | Stack | AOMDV, rules, agents, hybrid |
| | Fire | F1, F2, F3 |
| | Group of pictures (GOP) structure | 3-9, 6-6, 9-3 |
| Repetitions | Simulations | 20 per stack and per cell, 720 in all |

## Reproducing the study

```bash
cd experiments/run
for s in $(cat ../studies/09-comparative/sets.txt); do
  fire=${s##*-d40-}; fire=${fire%%-*}          # fire, fire2 or fire3
  bash run_sets.sh --dbg=0 --quiet-log --prune \
    --seed-name=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-$fire "$s"
done
cd ../../article/tools
python3 compile_comparative.py      # -> ../src/data/comparative.tsv
python3 analyse_comparative.py      # paired tests, Holm, ANOVA
python3 make_tables_comparative.py  # the tables, both editions
```

Four stacks (AOMDV, the rule stack, the agents stack, the hybrid stack),
three fires (F1, F2, F3) and three GOP structures (3-9, 6-6 and 9-3, set by
the GOP coefficient `gopCoef` of the encoder files `ipFiles/GC-PN-*.dat`)
form 36 sets of twenty simulations. The seed of a simulation derives from
the name of the fire and its index (`--seed-name` above), not from the stack
or the GOP, so the four stacks and the three GOPs of a fire undergo the same
failures and the comparisons are paired. Without `--prune`, a set keeps its
traces and decoded frames, several gigabytes per set.

## Reading the measurements

`comparative.tsv` holds one row per simulation: the fire, the GOP structure
(`split`: `3M9D`, `6M6D` or `9M3D`), the stack, the simulation index, the
PSNR at the sink averaged over the twelve frames (dB), the overall packet
loss, the loss of the M frames, of priority level M1, of levels M1 to M3 and
of the D frames (%), and the recoverable share of the losses given by the
connectivity oracle. Every
mean, confidence interval and test of the article is computed from it by
`analyse_comparative.py`, so each reported number can be checked without
rerunning a simulation.
