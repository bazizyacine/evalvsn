# EvalVSN — cross-layer video transmission over wireless sensor networks

Supporting material for the article *Optimized Cross-Layer Architecture for
Robust Video Transmission over Wireless Sensor Networks*.

| Directory | Contents |
|---|---|
| `src/` | the EvalVSN video codec (C++/OpenCV) |
| `ns-2-modules/` | the ns-2.35 modules: `aomdv_qpv/` (the contribution), `aomdv/` (the baseline), the EvalVSN MAC and PHY, the priority interface queue, and the `hdr_cmn` extension in `common/packet.h` |
| `scripts/` | the simulation driver, `sets.conf`, and the plotting chain |
| `ipFiles/` | encoder and decoder parameter files, per experiment set |
| `data/` | the measurement files behind every figure of the article |

## Reproducing the results

The two scenarios of the article are declared in `scripts/sets.conf`:

```bash
cd scripts
bash run_sets.sh --dbg=0 set_AOMDV_GC-PN set_AOMDV_QPV_GC-PN
```

Each set sweeps twelve operating points: the group-of-pictures coefficient over
{0.001, 0.011, 0.019, 0.03} and the number of priority levels over {1, 7, 13},
on 100 nodes with the `mobile_qcif` sequence.

The `.dat` files under `data/` are the outputs the article's tables and figures
were computed from, so every reported number can be checked without rerunning
the simulations.
