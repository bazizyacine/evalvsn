# The files this work adds to or modifies in ns-2.35

Each file sits at its path under `ns-2.35/` of the official release
(`ns-allinone-2.35`). The 802.15.4 model of the release (`wpan/`) is
untouched: the MAC and PHY of this work derive from it.

## Modules added

| Path under `ns-2.35/` | Role in the chain |
|---|---|
| `aomdv_qpv/` (`aomdv_qpv.{cc,h}`, `_rtable`, `_rqueue`, `_packet.h`, `_logs.cc`) | the cross-layer routing agent AOMDV_QPV, derived from the release's AOMDV: the reserve of paths with their cumulative cost, the per-class link cost, the congestion threshold read from the next hop's HELLO, the handling of MAC failures, the early discard by priority, and the learning agents of the path, the congestion, the link-break detection, the discovery and the early discard |
| `queue/priqueueEvalVSN.{cc,h}` | the interface queue that inserts, serves and drops by criticality rank, and asks the routing agent whether to admit a packet (early discard) |
| `mac/mac-802_15_4EvalVSN.{cc,h}` | the 802.15.4 MAC extended with the per-neighbour memory of link measures (LQI, SNR), the frame-outcome signal to the routing agent, and the learning agents of the channel access and the transmit power |
| `mac/phy-802_15_4EvalVSN.{cc,h}` | the 802.15.4 PHY extended with the link measures of each received packet and the closed-loop transmit power |
| `mac/power-monitorEvalVSN.{cc,h}` | the power monitor that keeps the interference account behind the measures |
| `mac/link-metrics.h` | the link-measure store the routing agent reads from the MAC |
| `mac/fire-propagationEvalVSN.{cc,h}` | the propagation model of the fire: the heated zone ahead of the front |
| `evalvsn/evalvsn.{cc,h}`, `evalvsnUDP.{cc,h}`, `evalvsnSink.{cc,h}` | the trace-driven video source with the criticality header, its UDP agent, and the sink that writes the reception trace and discards duplicates |

## Files of the release modified

| Path under `ns-2.35/` | Change |
|---|---|
| `common/packet.h` | the packet type `PT_AOMDV_QPV` (last before `PT_NTYPE`), the video fields of the common header (sequence number, size, frame, frame type, priority level, offset), the link measures the PHY writes (RSSI, SNR, LQI) and AOMDV_QPV's salvage count |
| `common/agent.{cc,h}` | the video fields every agent stamps on the packets it sends |
| `trace/cmu-trace.{cc,h}` | the trace format of AOMDV_QPV's routing packets |
| `tcl/lib/ns-agent.tcl`, `ns-lib.tcl`, `ns-mobilenode.tcl`, `ns-packet.tcl` | AOMDV_QPV declared as a routing agent and a packet header, as AOMDV is |
| `tcl/lib/ns-default.tcl` | one block at the end: the default values of the modules' bound variables, every agent's switch and the early discard at 0, the rule switches (`CLASS_COST_`, `HELLO_QUEUE_`) at 1 |
| `linkstate/ls.h` | `this->` in `eraseAll`, the compile fix a modern g++ needs |
| `Makefile.in` | the modules in the object list; g++ 4.8, which the release still compiles with |

