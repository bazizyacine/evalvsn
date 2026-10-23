# Gallery of the comparative study

Frame 8 of simulation r01 under the F1 fire at the 9-3 split of study 09,
decoded at the sink by `evalvsn/bin/evalvsn -decode` from the set's
encoding and the r01 traces (`st-packet.dat`, `rt-packet.dat`,
`ns2_st-packet.dat`, `st-frame.dat`). `tools/make_figures.py` lays the five
images out as the gallery figure.

| file | what it shows | decoded from |
|---|---|---|
| `reference.png` | the frame coded without network loss | the encoding (11 October 2026) |
| `aomdv.png` | classic AOMDV | `set_AOMDV_GC-PN-0p03-13_p9-n100-d40-fire-pq` |
| `rules.png` | the rule stack | `set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-fire-rules` |
| `agents.png` | the agents stack | `set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-fire-agents` |
| `hybrid.png` | the hybrid stack | `set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-fire-hybrid` |

The four frames were decoded on 16 October 2026 from the rerun of study 09
on the packets that decode alone (decision d53) and the corrected salvage
(finding X-18): the decoder reads only the packets that reached the sink.
The reference frame is unchanged, the coding itself being the same. The
decoding procedure reproduces a stored frame pixel for pixel.
