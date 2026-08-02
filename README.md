# EvalVSN — cross-layer video transmission over wireless sensor networks

Supporting material for the article *An Optimised Cross-Layer Approach for
Multimedia Transport in Wireless Sensor Networks*, submitted to *Sensors*
(MDPI).

| Directory | Contents |
|---|---|
| `evalvsn/src/` | the EvalVSN video codec (C++/OpenCV), with its `CMakeLists.txt` |
| `ns-2-modules/` | the ns-2.35 modules: `aomdv_qpv/` (the contribution), `aomdv/` (the baseline), the EvalVSN MAC and PHY, the priority interface queue, and the `hdr_cmn` extension in `common/packet.h` |
| `study/` | the experiments: `run/` drives the simulations, `metrics/` extracts the measurements, `sets.conf` declares them, `ipFiles/` holds the encoder and decoder parameters |
| `data/` | the measurement files behind every figure of the article |

The layout mirrors the one the work is done in, so the relative paths inside the
scripts resolve unchanged.

## Reproducing the results

Build the codec first (`cmake . && make` under `evalvsn/`), then:

```bash
cd study/run
bash run_sets.sh --dbg=0 --quiet-log --prune set_AOMDV_GC-PN set_AOMDV_QPV_GC-PN
```

Each set sweeps twelve operating points: the group-of-pictures coefficient over
{0.001, 0.011, 0.019, 0.03} and the number of priority levels over {1, 7, 13},
on 100 nodes with the `mobile_qcif` sequence.

`--dbg=0` suppresses the ns-2 cross-layer traces, which otherwise produce more
than a gigabyte of log per simulation for identical results. `--prune` deletes
the traces and decoded frames once the metrics are extracted; without it a
ten-realisation campaign leaves some 19 GB behind.

## Reading the measurements

Each set runs **ten independent realisations**. The encoding is shared across
them — it depends only on the video and the encoder parameters, not on the seed
— so ten realisations cost one encoding, ten simulations and ten decodings.
Seeds are derived from the set name and the realisation index, so a campaign
replays identically.

Every `.dat` file under `data/` therefore has four columns:

```
simId    mean    standard deviation    realisations
```

The spread is worth reading alongside the mean. On the delivery ratio it runs
from 1.45 to 3.86 points under plain AOMDV against 0.23 to 0.28 under
AOMDV_QPV: the cross-layer protocol is not only better on average but an order
of magnitude more reproducible, which a single run cannot show.

The measurements are the ones the article's tables and figures were computed
from, so every reported number can be checked without rerunning anything.
