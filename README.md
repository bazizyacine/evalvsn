# EvalVSN — reinforcement learning agents in a cross-layer stack for video over wireless sensor networks

Supporting material for the article *Reinforcement Learning Agents in a
Cross-Layer Stack for Forest Fire Video Surveillance over Wireless Sensor
Networks* (M. Y. Baziz, L. Sekhri and B. Kechar), under revision in
*Sensors* (MDPI). This release, v2.0.3, is the version of the revised
manuscript: the code that ran the simulations, the results of every
simulation and the commands that reproduce them.

| Directory | Contents |
|---|---|
| `evalvsn/` | the EvalVSN tool, which codes the video of the camera and evaluates it at the sink (C++17, OpenCV 4.6), with its `CMakeLists.txt` |
| `ns-2-modules/` | every file added to or modified in ns-2.35, at its path under `ns-2.35/`: the cross-layer routing agent `aomdv_qpv/` (AOMDV Quality Prioritized Video, AOMDV_QPV) with its learning agents, the derived 802.15.4 MAC and PHY, the power monitor and the fire propagation model under `mac/`, the priority interface queue under `queue/`, the video source and sink under `evalvsn/`, and the release files they touch, `aomdv/` among them; its `README.md` lists them, with the switches of the revision's experiments |
| `experiments/` | the simulation and measurement scripts: `run/` drives the simulations, `metrics/` extracts the measurements and holds the connectivity oracle, `ipFiles/` holds the encoder parameters, `sets.conf` declares every set; `studies/09-comparative/` (the 36 sets of the main study, experiment E0) and `studies/10-revision/` (the 261 sets of the other experiments) each hold `sets.txt`, a `README.md` and `results/`, with `runs.tsv`, one row per simulation |
| `article/tools/` | the compilation of the simulations, the analysis of the codec, the paired tests with Holm correction, the analyses of variance, and the tables and figures of the article |
| `article/src/data/` | the measurements behind every number of the article: `comparative.tsv` and `revision.tsv`, one row per simulation, and the files listed below |
| `article/src/figures-src/icons/` | the Twemoji pictograms of Figures 1 and 2 (Twitter, Inc and other contributors, CC BY 4.0, license beside them) |
| `LICENSE`, `LICENSE-data`, `ns-2-modules/LICENSE-GPL-2.0` | the licenses: GPL-3.0 for the EvalVSN tool and the scripts, GPL-2.0 or later for the files of the ns-2 modules written by the authors, CC BY 4.0 for the data (see License) |
| `CITATION.cff`, `.zenodo.json` | the reference of this release, and its record at Zenodo |

## The experimental plan

The article's plan (its Table 6) holds six experiments, E0 to E5. An
experiment with several series, levels or groups of variants of its factor,
tests each series within its own Holm family. A configuration is one set: one
stack, one fire, one GOP and one level of the factor, simulated as many times
as `experiments/sets.conf` declares; `article/tools/make_tables_plan.py`
counts them from the two `sets.txt` and writes the table.

| Experiment | Question | Configurations (simulations) | Sets |
|---|---|---|---|
| E0 | Effect of the stack, the fire and the GOP | 36 (720) | `experiments/studies/09-comparative/` |
| E1 | Implementation of AOMDV (series: control AOMDV, AOMDV with cross-layer timers) | 18 (360) | `experiments/studies/10-revision/` |
| E2 | Contribution of each decision | 39 (780) | `experiments/studies/10-revision/` |
| E3 | Adaptation of the agents (series: frozen, decaying ε) | 24 (480) | `experiments/studies/10-revision/` |
| E4 | Video coding, GOP volume and quality coefficient (series: equal rate, coefficient 0.1) | 24 (480) | `experiments/studies/10-revision/` |
| E5 | Reach of the lead of the hybrid stack (series: load, fire, scenario) | 150 (3000) | `experiments/studies/10-revision/` |

Two measurements without a code complete the plan (Section 2.7 of the
article). The agents' traces rerun 120 simulations of E0, those of
the agents and hybrid stacks at GOP 9-3 (6 sets of study 10),
with a trace of the agents' choices and values every 20 s. The costs for a
sensor node have no simulation of their own: they are measured on the
simulations of E0 to E3, of E5 and of the traces.

Study 10 also keeps the results of 12 sets (240 simulations)
that its `sets.txt` no longer lists and that the article does not read: the
first run of the quality coefficient 0.1, simulated for 165 s. The article
reads their replay at 168 s, in E4. Their declarations stay in
`experiments/sets.conf` (`experiments/studies/10-revision/README.md`).

## Building

1. ns-2.35 from `ns-allinone-2.35`: copy `ns-2-modules/` over `ns-2.35/`,
   then build as usual (`./configure && make` in `ns-2.35/`; `Makefile.in`
   compiles with gcc-4.8 and g++-4.8). The release's 802.15.4 classes are
   left untouched; the stack lives in derived classes.
2. The EvalVSN tool: `cmake . && make` under `evalvsn/`, which builds
   `bin/evalvsn`.
3. The videos: each content is a 12 s cut of a video of the FLAME 2 dataset
   (Chen et al., IEEE DataPort, 2022), which this repository does not
   redistribute: in the YUV file, 300 frames at 25 frames per second,
   QCIF (176 × 144 pixels), YUV 4:2:0, under `evalvsn/videos/`; the encoder
   reads the AVI file and codes twelve frames of each in grayscale (Table
   2 of the article).

   | Content | FLAME 2 video | From | Read by |
   |---|---|---|---|
   | `fire_qcif` | `#6) RGB Video 6.MP4` | 27.0 s | every experiment |
   | `forest_qcif` | `#18) RGB Preburn Video 2.MP4` | 60.0 s | E5 (scenario) |
   | `smoke_qcif` | `#7) RGB Video 7.MP4` | 170.0 s | E5 (scenario) |

   With FFmpeg 6.1.1, these two commands write the `fire_qcif` files the
   simulations read, byte for byte (their SHA-256 sums are given below):

   ```bash
   ffmpeg -ss 27.0 -t 12 -i "#6) RGB Video 6.MP4" -vf "fps=25,scale=176:144" \
     -frames:v 300 -pix_fmt yuv420p -f rawvideo evalvsn/videos/fire_qcif.yuv
   ffmpeg -ss 27.0 -t 12 -i "#6) RGB Video 6.MP4" -vf "fps=25,scale=176:144" \
     -pix_fmt yuv420p -q:v 2 evalvsn/videos/fire_qcif.avi
   ```

   The AVI of `fire_qcif` is cut from the FLAME 2 video itself, at the
   quantizer 2 (`-q:v 2`). Its header declares 301 frames
   (12.04 s): 300 coded frames and an empty entry after the
   first one. EvalVSN takes the frame count from the header
   (`CAP_PROP_FRAME_COUNT`, `evalvsn/src/evalvsn.cc`) and codes one frame in
   26 (Table 2 of the article). An AVI converted from
   `fire_qcif.yuv` declares 300 frames: EvalVSN would code one frame
   in 25, other images. The other contents are cut with the first
   command, from their video and instant in the table above, then converted
   with `ffmpeg -s qcif -i evalvsn/videos/<content>.yuv evalvsn/videos/<content>.avi`, which writes their AVI byte for byte.
   `experiments/run/run_sets.sh` converts the YUV file only when the AVI is
   missing or `--regen-avi` is passed: write `fire_qcif.avi` with the command
   above before running a set. The start of `fire_qcif` was found afterwards,
   by matching its first frame (mean squared error of 4.4).

## Configuration of the main study

As Tables 1 and 2 of the article write it.

Table 1. Network and simulation parameters, and factors of the main study E0.

| Category | Parameter | Value |
|---|---|---|
| Common configuration | Simulator | ns-2.35, IEEE 802.15.4 at 2.4 GHz, 250 kbit/s |
|  | Topology | grid of 10 × 10 nodes spaced 40 m apart, camera and sink at opposite corners |
|  | Radio | AT86RF231, transmission at +3 dBm, sensitivity of -101 dBm |
|  | Clear channel assessment (CCA) | threshold of -91 dBm |
|  | Propagation | log-distance with exponent 3.2, range of 48.6 m |
|  | Interface queue | 16 packets |
|  | Traffic | one reading of 32 bytes every 60 s throughout the mission; from detection at 60 s, one image every 9 s, in packets sent every 15 ms |
|  | Simulated duration | 168 or 169 s depending on the group of pictures (GOP) structure |
|  | Agents | learning rate of 0.3 (routing) and 0.2 (access), exploration of 0.1 |
| Studied factors | Stack | Ad hoc On-demand Multipath Distance Vector (AOMDV), rules, agents, hybrid |
|  | Fire | F1, F2, F3 (Table 3) |
|  | GOP structure | 3-9, 6-6, 9-3 (Table 2) |
| Repetitions | Simulations | 20 per stack, fire and GOP, 720 in all |

Table 2. Parameters of the EvalVSN tool.

| Parameter | Value |
|---|---|
| Videos | FLAME 2 (fire, forest before the burn, smoke), cut with FFmpeg 6.1.1; 176 × 144 pixels in grayscale, 12 frames |
| Encoder | Modified Moving Picture Experts Group (MMPEG), as implemented in EvalVSN |
| Frame rate | 1 image per second, hence 12 of the 301 images of the source, at 25 images per second, that is, one in 26, every 1.04 s (one in 25, every 1.00 s, for the 300 images of the forest and smoke videos of the scenario variants, Section 2.7) |
| Skip threshold | zero, no skipped image |
| GOP coefficient GC | 0.01597, 0.02525 or 0.03, that is, GOP 3-9, 6-6 or 9-3, a studied factor of E0 |
| Quality coefficient | 0.01 on the luminance table of the Joint Photographic Experts Group (JPEG) |
| Priority levels | 13, one per diagonal of the zigzag scan of a block, the first two and the last two merged |
| Transform | classical discrete cosine transform, on blocks of 8 × 8 pixels |
| Coefficient coding | run-length coding at all levels, then order-0 Exp-Golomb binary code |
| Payload | at most 74 bytes, header included |

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
M3 and of the D frames (%), and, in the column `recoverable`, the packets
lost while a path of live nodes linked the camera to the sink, in % of the
packets sent (the instantaneous-path losses of the article), given by the
connectivity oracle. Every mean, confidence interval and test of E0 is
computed from it by `analyse_comparative.py`, so each reported number can be
checked without rerunning a simulation.

## The raw results

This release archives the raw result of every simulation of studies 09 and
10, 720 and 5460 simulations: those of study 10 are the 5220
the article reads and the 240 of the first run of the quality
coefficient 0.1 at 165 s. The per-packet traces are not included.

### What each file of the measurements is

| File | Contents | Written by |
|---|---|---|
| `experiments/studies/09-comparative/results/runs.tsv` | the 720 simulations of study 09 (E0), one row each | `experiments/metrics/compile_runs.py` |
| `experiments/studies/10-revision/results/runs.tsv` | the 5460 simulations of study 10, one row each: the 5220 of the 261 sets of `sets.txt`, then the 240 of the 12 sets the article does not read (the first run of the quality coefficient 0.1, at 165 s) | `experiments/metrics/compile_runs.py` |
| `experiments/studies/*/results/sets.tsv` | one row per set, the 36 of study 09 and the 273 of study 10 (the 12 above included): its declaration, the mean, standard deviation, minimum and maximum over its realisations of each measure of `sim/mt.dat`, and the aggregates of `setGraphs.pl` for the others | `experiments/metrics/compile_sets.py` |
| `experiments/studies/09-comparative/results/routing.tsv` | route requests, route errors, MAC drops, route-queue refusals, route replies and hellos per simulation, AOMDV and rule stacks at GOP 9-3 | `experiments/metrics/routing_events.py` |
| `experiments/studies/10-revision/sets.txt`, `README.md` | the 261 sets of study 10, by experiment and series | |
| `article/src/data/comparative.tsv` | one row per simulation of E0 (see above) | `compile_comparative.py` |
| `article/src/data/fire-map-F1.tsv` to `fire-map-F3.tsv` | the state of each node at the instant the scenario figure draws, per fire | `compile_comparative.py` |
| `article/src/data/galerie/` | the frames of the gallery figure, decoded at the sink, and their `README.md` | `evalvsn/bin/evalvsn -decode` |
| `article/src/data/revision.tsv` | one row per simulation of studies 09 and 10 that the article reads: PSNR, SSIM, loss, M1 loss, usable images (PSNR ≥ 28 dB, and 25 and 31 dB), mean and 95th-percentile delay, late packets, neighbor-death declarations and the false ones, control messages | `compile_revision.py` |
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
simulation measured, as the tools wrote it: the `sim/mt.dat` columns (PSNR,
SSIM and their loss-free references in dB and [0, 1], packet loss overall, of the M
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
   needed from here on. Each table tool writes, under `article/src/tables/`,
   the article's tables and their French counterparts, their names starting
   with `en_` and `fr_`; the figure tools write `article/src/figures/`.

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
| 1, Network and simulation parameters, and factors of the main study E0 | written in the article | |
| 2, Parameters of the EvalVSN tool | written in the article | |
| 3, Three fires; camera at (0, 0) m, sink at (360, 360) m | written in the article | |
| 4, The agents and the rules they replace | written in the article | |
| 5, Metrics used in the results | written in the article | |
| 6, Experimental plan, twenty simulations per stack, fire, GOP and factor level | `make_tables_plan.py` |  |
| 7, PSNR at the sink, mean ± half-width of the 95% confidence interval | `make_tables_comparative.py` | `analyse_comparative.py` |
| 8, Ablations at GOP 9-3: agent minus its rule, means over 60 seeds | `make_tables_ablation.py` | `analyse_ablation.py` |
| 9, Agent decisions at GOP 9-3, means per simulation | `make_tables_revision.py` |  |
| 10, Controls at GOP 9-3, means over 60 seeds | `make_tables_revision.py` | `analyse_revision.py` |
| 11, Paired PSNR differences between the cross-layer stacks, means per cell | `make_tables_comparative.py` | `analyse_comparative.py` |
| 12, Usable images, delay and missed deadlines of the four stacks, means over 60 seeds | `make_tables_quality.py` | `make_tables_quality.py --stats` |
| 13, Repeated-measures analysis of the PSNR, 540 simulations over 60 seeds | `make_tables_comparative.py` | `analyse_comparative.py`; `mixed_model.py`, the mixed-model check |
| 14, GOP at T = 9 s and at equal rate, means over 60 seeds, T denoting the alarm period | `make_tables_volume.py` | `analyse_claims.py`, rows P26, the tests at equal rate |
| 15, Quality coefficient (0.01 or 0.1) at GOP 9-3, means over 60 seeds | `make_tables_volume.py` | `analyse_qualite.py`, the tests of the coefficient 0.1 |
| 16, Alarm period T at GOP 9-3, means over 60 seeds | `make_tables_charge.py` |  |
| 17, Fire parameters at GOP 9-3, means over 60 seeds | `make_tables_sensibilite.py` | `analyse_sensibilite.py` |
| 18, Scenario variants at GOP 9-3, means over 60 seeds, reference: fire video, 40 m grid | `make_tables_generalisation.py` | `analyse_generalisation.py` |
| 19, Energy, control messages and memory at GOP 9-3, means per simulation over 60 seeds | `make_tables_overhead.py` | `make_tables_overhead.py` |

### Each figure of the revised article

| Figure | Drawn by |
|---|---|
| 1, Video surveillance of a forest fire by a wireless sensor network | a drawing (TikZ or UML), not part of this release |
| 2, The three fires 90 s after detection, in the first simulation of the rule stack | `make_figures.py` |
| 3, Admission region of the early discard as a function of the congestion index IC and the normalized rank of the packet, for the threshold IC_0 of the rule and the other finite thresholds the agent can choose | `make_figures.py` |
| 4, The cross-layer stack extended from Ad hoc On-demand Multipath Distance Vector (AOMDV) routing, named AOMDV Quality Prioritized Video (AOMDV_QPV) | a drawing (TikZ or UML), not part of this release |
| 5, Unified Modeling Language (UML) class diagram of the agents | a drawing (TikZ or UML), not part of this release |
| 6, Learning trajectories of the agents at GOP 9-3 | `make_figure_trajectories.py` |
| 7, PSNR at the sink of the four stacks by GOP and fire | `make_figures.py` |
| 8, Loss of the M1 level of the four stacks by GOP and fire | `make_figures.py` |
| 9, Image 8 of the first simulation under F1 at GOP 9-3 | `make_figures.py` |

`analyse_claims.py` recomputes every other number the results section quotes,
from `article/src/data/`, and prints for each the value the text writes, the
value computed and whether they agree; it then runs
`analyse_claims_generalisation.py` (the scenario series of E5) and
`analyse_claims_aomdv.py` (the tuned AOMDV series of E1), whose lines read
the same way. Their labels are in French, and their numbers carry a decimal
comma; the numbers are those of the article. Five lines of
`analyse_claims_aomdv.py` print MISSING (A09 to A11): they need the simulation
sets themselves (`PROGRESS.txt`) or an energy of tuned AOMDV that
`overhead.tsv` does not hold, and the article prints none of their numbers.

`make_tables_overhead.py` reads the size of the agents' tables from the
simulator's headers: from ns-2.35 if it is unpacked at
`../ns-2/ns-allinone-2.35/ns-2.35/` beside the repository, else from
`ns-2-modules/`.

## License

The licenses go by directory:

| Directory | License |
|---|---|
| `evalvsn/` (the EvalVSN tool), `experiments/` but its `results/`, `article/tools/` | GNU General Public License, version 3 (`LICENSE`) |
| `ns-2-modules/`, the 9 files written by the authors (`SPDX-License-Identifier: GPL-2.0-or-later` in their header) | GNU General Public License, version 2 or any later version (`ns-2-modules/LICENSE-GPL-2.0`), so that they combine with the GPL version 2 of AOMDV, with which they are compiled |
| `ns-2-modules/`, the 31 files that come from ns-2.35 or derive from it, AOMDV and AOMDV_QPV among them | their original license, as the notices of their headers state it; `ns-2-modules/README.md` lists each file by license |
| `experiments/studies/*/results/`, `article/src/data/` and the tables the tools generate from them | Creative Commons Attribution 4.0 International (`LICENSE-data`), except the frames of `article/src/data/galerie/`, decoded from a FLAME 2 video, which keep the dataset's terms |
| `article/src/figures-src/icons/` | the Twemoji pictograms, by Twitter, Inc and other contributors, under CC BY 4.0 (`LICENSE-GRAPHICS` beside them) |

## Citation

This release, v2.0.3, is archived at Zenodo. All its versions share the
concept DOI [10.5281/zenodo.24147853](https://doi.org/10.5281/zenodo.24147853), which
leads to the latest one; the Zenodo record of each version gives the DOI of
that version.
`CITATION.cff` gives the reference of this release in a form citation
managers read; the article's reference will join it once the article is
published.
