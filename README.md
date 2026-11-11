# EvalVSN — reinforcement learning agents in a cross-layer stack for video over wireless sensor networks

Supporting material for the article *Reinforcement Learning Agents in a
Cross-Layer Stack for Forest Fire Video Surveillance over Wireless Sensor
Networks* (M. Y. Baziz, L. Sekhri and B. Kechar), submitted to *Sensors*
(MDPI). This release, v2.0.0, is the version of the revised manuscript:
the code that ran the simulations, the results of every simulation and the
commands that reproduce them.

| Directory | Contents |
|---|---|
| `evalvsn/` | the EvalVSN video codec and evaluation chain (C++17, OpenCV 4.6), with its `CMakeLists.txt` |
| `ns-2-modules/` | every file added to or modified in ns-2.35, at its path under `ns-2.35/`: the cross-layer routing agent `aomdv_qpv/` (AOMDV Quality Prioritized Video, AOMDV_QPV) with its learning agents, the derived 802.15.4 MAC and PHY, the power monitor and the fire propagation model under `mac/`, the priority interface queue under `queue/`, the video source and sink under `evalvsn/`, and the release files they touch, `aomdv/` among them; its `README.md` lists them, with the switches of the revision's experiments |
| `experiments/` | the simulation chain: `run/` drives the simulations, `metrics/` extracts the measurements and holds the connectivity oracle, `ipFiles/` holds the encoder parameters, `sets.conf` declares every set; `studies/09-comparative/` (the 36 sets of the main study, experiment E0) and `studies/10-revision/` (the 261 sets of the other experiments, 261 of which the article reads) each hold `sets.txt`, a `README.md` and `results/runs.tsv`, one row per simulation |
| `article/tools/` | the compilation of the simulations, the analysis of the codec, the paired tests with Holm correction, the analyses of variance, and the tables and figures of the article |
| `article/src/data/` | the measurements behind every number of the article: `comparative.tsv` and `revision.tsv`, one row per simulation, and the files listed below |
| `article/src/figures-src/icons/` | the Twemoji pictograms the scenario figure is drawn with (Twitter, Inc. and other contributors, CC BY 4.0, licence beside them) |
| `LICENSE`, `LICENSE-data` | the licences: GPL-3.0 for the code, CC BY 4.0 for the data (see Licence) |
| `CITATION.cff`, `.zenodo.json` | the reference of this release, and its record at Zenodo |

## The experimental plan

The article's plan (its Table 4) holds six experiments, E0 to E5,
and two measures named without a code. An experiment with several series,
levels or groups of variants of its factor, tests each series within its own
Holm family. A configuration is one set: one stack, one fire, one GOP and one
level of the factor, simulated as many times as `experiments/sets.conf`
declares; `article/tools/make_tables_plan.py` counts them from the two
`sets.txt` and writes the table.

| Experiment | Question | Configurations (simulations) | Sets |
|---|---|---|---|
| E0 | Effect of the stack, the fire and the GOP | 36 (720) | `experiments/studies/09-comparative/` |
| E1 | AOMDV's implementation (series: control AOMDV, AOMDV with cross-layer timers) | 18 (360) | `experiments/studies/10-revision/` |
| E2 | Contribution of each decision | 39 (780) | `experiments/studies/10-revision/` |
| E3 | Agents' adaptation (series: frozen, decaying ε) | 24 (480) | `experiments/studies/10-revision/` |
| E4 | Video coding, GOP volume and quality coefficient (series: equal rate, coefficient 0.1) | 24 (480) | `experiments/studies/10-revision/` |
| E5 | Extent of the hybrid stack's lead (series: load, fire, scenario) | 150 (3000) | `experiments/studies/10-revision/` |
| – | Agents' traces | 6 (120) | `experiments/studies/10-revision/` |
| – | Overheads for a node | no simulation, measures of E0 to E3, E5 and the traces |  |

Study 10 also keeps 0 sets (0 simulations) that the article
does not read: the first run of the quality coefficient 0.1, which stopped at
165 s; the article reads its replay at 168 s, in E4
(`experiments/studies/10-revision/README.md`).

## Building

1. ns-2.35 from `ns-allinone-2.35`: copy `ns-2-modules/` over `ns-2.35/`,
   then build as usual (`./configure && make` in `ns-2.35/`; `Makefile.in`
   compiles with gcc-4.8 and g++-4.8). The release's 802.15.4 classes are
   left untouched; the stack lives in derived classes.
2. The codec: `cmake . && make` under `evalvsn/`, which builds `bin/evalvsn`.
3. The videos: each content is a 12 s cut of a video of the FLAME 2 dataset
   (Chen et al., IEEE DataPort, 2022), which this repository does not
   redistribute: 300 frames at 25 frames per second, QCIF (176 × 144 pixels),
   YUV 4:2:0, under `evalvsn/videos/`; the encoder codes twelve frames of each
   in grayscale (Table 3 of the article).

   | Content | FLAME 2 video | From | Read by |
   |---|---|---|---|
   | `fire_qcif` | `#6) RGB Video 6.MP4` | 27.0 s | every experiment |
   | `forest_qcif` | `#18) RGB Preburn Video 2.MP4` | 60.0 s | E5 (scenario) |
   | `smoke_qcif` | `#7) RGB Video 7.MP4` | 170.0 s | E5 (scenario) |

   ```bash
   ffmpeg -ss 27.0 -t 12 -i "#6) RGB Video 6.MP4" -vf "fps=25,scale=176:144" \
     -frames:v 300 -pix_fmt yuv420p -f rawvideo evalvsn/videos/fire_qcif.yuv
   ffmpeg -s qcif -i evalvsn/videos/fire_qcif.yuv evalvsn/videos/fire_qcif.avi
   ```

   The other contents are cut the same way, from the instants above. The
   start of `fire_qcif` was found afterwards, by matching its first frame
   (mean squared error of 4.4), and the AVI may depend on the ffmpeg version:
   a new cut can differ from the files the simulations read, whose SHA-256
   sums are given below.

## Configuration of the main study

As Table 3 of the article writes it.

| Category | Parameter | Value |
|---|---|---|
| Common configurations | Simulator | ns-2.35, IEEE 802.15.4 at 2.4 GHz, 250 kbit/s |
|  | Topology | grid of 10 × 10 nodes spaced 40 m apart, camera and sink at opposite corners |
|  | Radio | AT86RF231, transmission at +3 dBm, sensitivity of -101 dBm |
|  | Clear channel assessment (CCA) | threshold of -91 dBm |
|  | Propagation | log-distance with exponent 3.2, range of 48.6 m |
|  | Interface queue | 16 packets |
|  | Video | FLAME 2, 176 × 144 pixels in grayscale, 12 frames |
|  | Encoder | MMPEG; one image in 26 from the source, that is, one every 1.04 s; skip threshold of zero, no skipped image; classical discrete cosine transform on 8 × 8 pixels; quality coefficient of 0.01 on the luminance table of the Joint Photographic Experts Group (JPEG); 13 priority levels; run-length coding at all levels, then Exp-Golomb; packets of at most 74 bytes |
|  | Traffic | one 32 byte reading every 60 s, then, from detection at 60 s, one image every 9 s, in packets sent every 15 ms |
|  | Simulated duration | 168 or 169 s depending on the group of pictures (GOP) structure |
|  | Agents | learning rate of 0.3 (routing) and 0.2 (access), exploration of 0.1 |
| Studied factors | Stack | AOMDV, rules, agents, hybrid |
|  | Fire | F1, F2, F3 (Table 1) |
|  | GOP structure | 3-9, 6-6, 9-3 |
| Repetitions | Simulations | 20 per stack and per cell, 720 in all |

## Reproducing the main study (E0)

```bash
cd experiments/run
for s in $(cat ../studies/09-comparative/sets.txt); do
  fire=$(sed -E 's/.*-d[0-9]+-(fire[23]?)(-.*)?$/\1/' <<<"$s")   # fire, fire2 or fire3
  bash run_sets.sh --seed-name=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-$fire "$s"
done
cd ../../article/tools
python3 compile_comparative.py      # -> ../src/data/comparative.tsv
python3 analyse_comparative.py      # paired tests, Holm, ANOVA
python3 make_tables_comparative.py  # the tables, both editions
```

The stacks (AOMDV, the rule stack, the agents stack, the hybrid stack), the
fires (F1, F2, F3) and the GOP structures (3-9, 6-6 and 9-3, set by the GOP
coefficient `gopCoef` of the encoder files `ipFiles/GC-PN-*.dat`) cross into
36 sets of 20 simulations. The seed of a simulation derives from the
name of the fire and its index (`--seed-name` above), not from the stack or
the GOP, so the stacks and the GOPs of a fire undergo the same failures and
the comparisons are paired. A set keeps its traces and decoded frames,
several gigabytes per set; do not pass `--prune`, which deletes the routing
trace (`aomdv_qpv.tr`) the compilation of the revision reads.

## Reading the measurements

`comparative.tsv` holds one row per simulation of E0: the fire, the GOP
structure (`split`: `3M9D`, `6M6D` or `9M3D`), the stack, the simulation
index, the PSNR at the sink averaged over the twelve frames (dB), the overall
packet loss, the loss of the M frames, of priority level M1, of levels M1 to
M3 and of the D frames (%), and the recoverable share of the losses given by
the connectivity oracle. Every mean, confidence interval and test of E0 is
computed from it by `analyse_comparative.py`, so each reported number can be
checked without rerunning a simulation.

## The raw results

This release archives the raw result of every simulation of studies 09 and
10, 720 and 5220 simulations; the per-packet traces are not
included.

### What each file of the measurements is

| File | Contents | Written by |
|---|---|---|
| `experiments/studies/09-comparative/results/runs.tsv` | the 720 simulations of study 09 (E0), one row each | `experiments/metrics/compile_runs.py` |
| `experiments/studies/10-revision/results/runs.tsv` | the 5220 simulations of study 10, one row each, the 0 the article does not read included | `experiments/metrics/compile_runs.py` |
| `experiments/studies/09-comparative/results/routing.tsv` | route requests, route errors, MAC drops, route-queue refusals, route replies and hellos per simulation, AOMDV and rule stacks at GOP 9-3 | `experiments/metrics/routing_events.py` |
| `experiments/studies/10-revision/sets.txt`, `README.md` | the 261 sets of study 10, by experiment and series | |
| `article/src/data/comparative.tsv` | one row per simulation of E0 (see above) | `compile_comparative.py` |
| `article/src/data/fire-map-F1.tsv` to `fire-map-F3.tsv` | the state of each node at the instant the scenario figure draws, per fire | `compile_comparative.py` |
| `article/src/data/galerie/` | the frames of the gallery figure, decoded at the sink, and their `README.md` | `evalvsn/bin/evalvsn -decode` |
| `article/src/data/revision.tsv` | one row per simulation of studies 09 and 10 that the article reads: PSNR, SSIM, loss, M1 loss, usable images (PSNR ≥ 28 dB, and 25 and 31 dB), mean and 95th-percentile delay, late packets, neighbour-death declarations and the false ones, control messages | `compile_revision.py` |
| `article/src/data/agent-trace.tsv` | the agents' decisions every 20 s in the 120 simulations of the agents' traces: deciding nodes, decisions, median per node, share away from the initial choice, decisions per action | `compile_agent_trace.py` |
| `article/src/data/break-agent.tsv` | per simulation at GOP 9-3: the failure counts the link-break agent chose, the declarations per 18 s of the alarm, the PSNR of each image | `compile_break_agent.py` |
| `article/src/data/frames.tsv` | per image, at alarm periods of 7, 9 and 12 s: type, PSNR, SSIM, packet loss and loss of the first priority level | `compile_frames.py` |
| `article/src/data/oracle-s10.tsv` | per simulation of the load, fire and scenario series of E5: packets lost, and those lost while no path of live nodes joined camera and sink | `compile_oracle.py` |
| `article/src/data/overhead.tsv` | radio energy per node and per network, and lifetime, per set at GOP 9-3 | `make_tables_overhead.py --compile` |
| `article/src/data/methode-mesures.tsv` | the numbers the methods quote from the sets: simulated time and nodes out of service per alarm period, the worst-case connectivity, the correlation of each loss class with the PSNR, the frame sizes and reference PSNR per GOP, the calibration losses, the packets of the other videos | `methode_mesures.py` |
| `article/src/data/codec-frames.tsv` | why a D frame of the fire video holds more bytes than an M frame: frames, bytes and packets per type and GOP, the same image coded as M and as D, the correlation and the energy of the DCT coefficients per band, the non-zero coefficients, the Exp-Golomb bits, the quality coefficient swept at GOP 9-3, the steps of the source video | `analyse_codec.py` |
| `experiments/metrics/compile_runs.py`, `routing_events.py` | the raw-results compiler and the routing-event counter | |

`runs.tsv` keys each row by `set` and `realisation`, gives the `seed` the
simulation ran on, the keys of `revision.tsv` (`arm`, `fire`, `split`,
`period`, `dist`; empty for a set the article does not read), then what the
chain measured, as it wrote it: the `sim/mt.dat` columns (PSNR, SSIM and
their loss-free references in dB and [0, 1], packet loss overall, of the M
and D frames and of each priority level M1 to M13 and D1 to D13, in %), the
PSNR, SSIM and loss of each image at the sink (`psnr_i1` … `loss_i12`), and
the routing counters where they were counted. A row of `revision.tsv` or
`comparative.tsv` joins its row in `runs.tsv` on these keys; its PSNR and
losses are the same numbers.

### The seeds

Every set of studies 09 and 10 takes the seeds of
`set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-<fire>`, `<fire>` being `fire`,
`fire2` or `fire3`: the seed of realisation k is
`cksum("<that name>-<k>") mod 900000 + 1` (`seed_for` in
`experiments/run/run_sets.sh`), the column `seed` of `runs.tsv`, checked
against the seed each set's log recorded.

### The exact commands, in order

Prerequisites: ns-2.35 built with `ns-2-modules/` (see Building) and on the
`PATH`, `evalvsn/bin/evalvsn`, perl, ffmpeg (ffprobe included), gnuplot,
Python 3.12 with NumPy, Matplotlib and Pillow (pandas and statsmodels for
`mixed_model.py`), and the videos under `evalvsn/videos/`. The files the
simulations read have these SHA-256 sums:

| File | SHA-256 |
|---|---|
| `fire_qcif.yuv` | `b660fdd2d63d42095168fb4a3fa273d92fe6250f05244bb4c3ef05471e9dc31d` |
| `fire_qcif.avi` | `79613269d72d46e506f1b391868b41509efc6c6323712b7865e8cc7fa7edaf9a` |
| `forest_qcif.yuv` | `3a29ba244832a083c638c744813ef4b7c2e04f038b35e2142309f5b10c693b15` |
| `forest_qcif.avi` | `0ec5f187b31270d3c97b15cd36b54d70ca6e94b65ea018012ea3b934bdeb407e` |
| `smoke_qcif.yuv` | `aa4daeb4bb3bec1a42c058e0e77950b598d08c6660111cc817215187b1b3dc60` |
| `smoke_qcif.avi` | `c53d4bf76a485b734d5daec719a0bc14a5fa25fda715ea5e9c02a369f5f697ab` |

1. The simulations, the commands the campaigns ran (each set ran with the
   options above; a set interrupted by a restart was completed with
   `--resume`, which keeps the realisations already harvested):

   ```bash
   cd experiments/run
   BASE=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40
   for study in 09-comparative 10-revision; do
     for s in $(cat ../studies/$study/sets.txt); do
       fire=$(sed -E 's/.*-d[0-9]+-(fire[23]?)(-.*)?$/\1/' <<<"$s")
       bash run_sets.sh --seed-name=$BASE-$fire "$s"
     done
   done
   ```

2. The routing counters, read from the packet traces (`st-packet.tr`); once
   they are counted, `st-packet.tr` and `st-packet.nam` may be deleted, no
   other tool reads them:

   ```bash
   cd ../metrics
   python3 routing_events.py -o ../studies/09-comparative/results/routing.tsv \
     set_AOMDV_GC-PN-0p03-13_p9-n100-d40-fire{,2,3}-pq set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-fire{,2,3}-rules
   for s in $(cat ../studies/10-revision/sets.txt); do
     python3 routing_events.py -o ../sets/$s/events.tsv "$s"
   done
   ```

3. The raw results, one row per simulation:

   ```bash
   python3 compile_runs.py --seed-base=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40 09-comparative
   python3 compile_runs.py --seed-base=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40 10-revision
   ```

4. The measurements of the article, from the sets (`article/tools/`):

   ```bash
   cd ../../article/tools
   python3 compile_comparative.py           # comparative.tsv, fire-map-F1.tsv to F3
   python3 compile_revision.py              # revision.tsv
   python3 compile_agent_trace.py           # agent-trace.tsv
   python3 compile_break_agent.py           # break-agent.tsv
   python3 compile_frames.py                # frames.tsv
   python3 compile_oracle.py                # oracle-s10.tsv
   python3 make_tables_overhead.py --compile   # overhead.tsv, then its table
   python3 methode_mesures.py               # methode-mesures.tsv
   python3 analyse_codec.py                 # codec-frames.tsv (--encoder: also re-encodes the video)
   ```

5. The tables and figures, from `article/src/data/` alone: no simulation is
   needed from here on. Each table tool writes the French and English
   editions under `article/src/tables/`, the figure tools write
   `article/src/figures/`.

   ```bash
   mkdir -p ../src/tables ../src/figures
   python3 make_tables_plan.py
   python3 make_tables_comparative.py
   python3 make_tables_revision.py
   python3 make_tables_ablation.py
   python3 make_tables_quality.py
   python3 make_tables_volume.py
   python3 make_tables_charge.py
   python3 make_tables_sensibilite.py
   python3 make_tables_generalisation.py
   python3 make_tables_overhead.py
   python3 make_figures.py --lang en
   python3 make_figure_trajectories.py --lang en
   ```

### Each table of the revised article

| Table | Generated by | Statistics printed by |
|---|---|---|
| 1, The three fires of the scenario | written in the article | |
| 2, The agents and the rules they replace | written in the article | |
| 3, Common configuration of the experiments and factors of the main study E0 | written in the article | |
| 4, Experimental plan | `make_tables_plan.py` |  |
| 5, Ablations at GOP 9-3, mean differences over 60 seeds | `make_tables_ablation.py` | `analyse_ablation.py` |
| 6, Controls at GOP 9-3, means over 60 seeds | `make_tables_revision.py` | `analyse_revision.py` |
| 7, Agent decisions at GOP 9-3, per simulation | `make_tables_revision.py` |  |
| 8, Peak signal-to-noise ratio (PSNR) at the sink (dB), mean ± half-width of the 95% confidence interval | `make_tables_comparative.py` | `analyse_comparative.py` |
| 9, Paired PSNR differences between the cross-layer stacks | `make_tables_comparative.py` | `analyse_comparative.py` |
| 10, Usable images, delay and missed deadlines of the four stacks, means over 60 seeds | `make_tables_quality.py` | `make_tables_quality.py --stats` |
| 11, Repeated-measures analysis of the PSNR of the three cross-layer stacks, 540 simulations over 60 seeds | `make_tables_comparative.py` | `analyse_comparative.py`; `mixed_model.py`, the mixed-model check |
| 12, GOP and quality coefficient, means over 60 seeds | `make_tables_volume.py` | `analyse_qualite.py`, the tests of the coefficient 0.1 |
| 13, Alarm period T at GOP 9-3, means over 60 seeds | `make_tables_charge.py` |  |
| 14, Sensitivity to the fire parameters at GOP 9-3, means over 60 seeds | `make_tables_sensibilite.py` | `analyse_sensibilite.py` |
| 15, Scenario variants at GOP 9-3, means over 60 seeds | `make_tables_generalisation.py` | `analyse_generalisation.py` |
| 16, Energy, control messages and memory at GOP 9-3, means per simulation over 60 seeds | `make_tables_overhead.py` | `make_tables_overhead.py` |

### Each figure of the revised article

| Figure | Drawn by |
|---|---|
| 1, Video surveillance of a forest fire by a wireless sensor network | a drawing (TikZ or UML), not part of this release |
| 2, The three fires 90 s after detection, in the first simulation of the rule stack | `make_figures.py` |
| 3, Admission region of the early discard as a function of the congestion index IC and the normalized rank of the packet, for the threshold IC_0 = 0.2 of the rule and for the other thresholds the agent can choose | `make_figures.py` |
| 4, The cross-layer stack extended from Ad hoc On-demand Multipath Distance Vector (AOMDV) routing, named AOMDV Quality Prioritized Video (AOMDV_QPV) | a drawing (TikZ or UML), not part of this release |
| 5, Unified Modeling Language (UML) class diagram of the agents | a drawing (TikZ or UML), not part of this release |
| 6, Learning trajectories of the agents at GOP 9-3 | `make_figure_trajectories.py` |
| 7, PSNR at the sink of the four stacks as a function of the GOP | `make_figures.py` |
| 8, Loss of the M1 level of the four stacks as a function of the GOP | `make_figures.py` |
| 9, Image 8 of the first simulation under F1 at GOP 9-3 | `make_figures.py` |

`analyse_claims.py` recomputes every other number the results section quotes,
from `article/src/data/`, and prints for each the value the text writes, the
value computed and whether they agree; it then runs
`analyse_claims_generalisation.py` (the scenario series of E5) and
`analyse_claims_aomdv.py` (the tuned AOMDV series of E1), whose lines read
the same way. Their labels, and the values as the text writes them, are in
French, as in the French edition of the article; the numbers are those of
both editions.

`make_tables_overhead.py` reads the size of the agents' tables from the
simulator's headers: from ns-2.35 if it is unpacked at
`../ns-2/ns-allinone-2.35/ns-2.35/` beside the repository, else from
`ns-2-modules/`.

## Licence

The code (`evalvsn/`, `ns-2-modules/`, `experiments/` but its `results/`, and
`article/tools/`) is distributed under the GNU General Public License,
version 3 (`LICENSE`). The files of `ns-2-modules/` that come from ns-2.35 or
derive from it keep the copyright and licence notices of their headers.

The data, `experiments/studies/*/results/`, `article/src/data/` and the tables
the tools generate from them, are distributed under the Creative Commons
Attribution 4.0 International licence (`LICENSE-data`), except the frames of
`article/src/data/galerie/`, decoded from a FLAME 2 video, which keep the
dataset's terms. The Twemoji pictograms of `article/src/figures-src/icons/`
are by Twitter, Inc. and other contributors, under CC BY 4.0
(`LICENSE-GRAPHICS` beside them).

## Citation

Please cite the article. This release, v2.0.0, is archived at Zenodo; its
DOI is given in the article's data availability statement. `CITATION.cff`
gives the reference in a form citation managers read.
