# The files this work adds to or modifies in ns-2.35

Each file sits at its path under `ns-2.35/` of the official release
(`ns-allinone-2.35`). The 802.15.4 model of the release (`wpan/`) is
untouched: the MAC and PHY of this work derive from it.

## Modules added

| Path under `ns-2.35/` | Role in the simulator |
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
| `aomdv/aomdv.{cc,h}`, `aomdv/aomdv_rqueue.h` | `HELLO_INTERVAL`, `MAX_RREQ_TIMEOUT` and `ACTIVE_ROUTE_TIMEOUT` bound to Tcl with the release's values as defaults (1, 1 and 10 s, in `ns-agent.tcl`), and a `set-rtq-max-len` command for the route queue (64 by default); see the switches of the revision below |
| `linkstate/ls.h` | `this->` in `eraseAll`, the compile fix a modern g++ needs |
| `Makefile.in` | the modules in the object list; g++ 4.8, which the release still compiles with |

## The switches of the revision's experiments

All are off by default, so that the default values replay the earlier runs
bit for bit; `experiments/run/evalvsn.tcl` sets them from a set's knobs
(`sets.conf`, last column). The experiments are those of the article's plan,
listed in `experiments/studies/10-revision/README.md`.

| Knob (`sets.conf`) | Bound variable | What it does |
|---|---|---|
| `rl_alpha=0`, `rl_epsilon=0`, `rl_mac_alpha=0`, `rl_mac_epsilon=0` | `RL_ALPHA_`, `RL_EPSILON_`, `RL_MAC_ALPHA_`, `RL_MAC_EPSILON_` | the frozen policy of E3: no learning and no exploration, each agent keeps its initial choice |
| `rl_eps_start`, `rl_eps_end` | `RL_EPS_START_`, `RL_EPS_END_` | the decaying exploration of E3: the rate falls linearly to 0 between the two instants (s), on the routing agent and the derived MAC |
| `rl_trace=1` | `RL_TRACE_` | the agents' traces: each agent's counters and values written to the routing trace every 20 s (`Q` lines); it only reads, and draws no random number |
| `rl_break_eps` | `RL_BREAK_EPSILON_` | the link-break detection agent's own exploration rate; below 0, the default, it follows the common rate |
| `aomdv_derived=1` | | classic AOMDV on the derived PHY and MAC, the control AOMDV of E1; never the reference |
| `aomdv_tuned=1` | `HELLO_INTERVAL_`, `MAX_RREQ_TIMEOUT_`, `ACTIVE_ROUTE_TIMEOUT_`, `set-rtq-max-len` | the tuned AOMDV of E1: AOMDV with the cross-layer stacks' routing timers and route queue (hellos every 5 s, 10 s route-request cap, 35 s active route, 16 packets waiting for a route) instead of the release's (1 s, 1 s, 10 s, 64); the release's values stay the defaults |

After a change to a header, rebuild ns-2.35 from scratch (`make clean`, then
`make`): the release's Makefile does not track header dependencies.

## Licenses

Each file keeps the license its header states.

| License | Files |
|---|---|
| GNU GPL, version 2 or any later version (`LICENSE-GPL-2.0`): the files written by the authors | `mac/fire-propagationEvalVSN.cc`, `mac/fire-propagationEvalVSN.h`, `mac/link-metrics.h`, `mac/mac-802_15_4EvalVSN.cc`, `mac/mac-802_15_4EvalVSN.h`, `mac/phy-802_15_4EvalVSN.cc`, `mac/phy-802_15_4EvalVSN.h`, `queue/priqueueEvalVSN.cc`, `queue/priqueueEvalVSN.h` |
| GNU GPL, version 2, with the ns-2 linking exception (M. Caleffi, 2008), and Carnegie Mellon University's notice (1997, 1998): AOMDV, and AOMDV_QPV, which derives from it | `aomdv/aomdv.cc`, `aomdv/aomdv.h`, `aomdv/aomdv_rqueue.h`, `aomdv_qpv/aomdv_qpv.cc`, `aomdv_qpv/aomdv_qpv.h`, `aomdv_qpv/aomdv_qpv_logs.cc`, `aomdv_qpv/aomdv_qpv_packet.h`, `aomdv_qpv/aomdv_qpv_rqueue.cc`, `aomdv_qpv/aomdv_qpv_rqueue.h`, `aomdv_qpv/aomdv_qpv_rtable.cc`, `aomdv_qpv/aomdv_qpv_rtable.h` |
| GNU GPL, version 2 or any later version, with the ns-2 linking exception (Xerox Corporation, 1997): derived from `ns-2.35/trace/traffictrace.cc` and `ns-2.35/apps/udp.{cc,h}` | `evalvsn/evalvsn.cc`, `evalvsn/evalvsn.h`, `evalvsn/evalvsnSink.cc`, `evalvsn/evalvsnSink.h`, `evalvsn/evalvsnUDP.cc`, `evalvsn/evalvsnUDP.h` |
| GNU GPL, version 2, with the ns-2 linking exception (Mercedes-Benz Research & Development North America and University of Karlsruhe, 2007): derived from `ns-2.35/mac/wireless-phyExt.{cc,h}` | `mac/power-monitorEvalVSN.cc`, `mac/power-monitorEvalVSN.h` |
| GNU GPL, version 2, with the ns-2 linking exception (University of Southern California, 2000) | `linkstate/ls.h` |
| the BSD-style license of the Regents of the University of California, as in ns-2.35 | `Makefile.in`, `common/agent.cc`, `common/agent.h`, `common/packet.h`, `tcl/lib/ns-agent.tcl`, `tcl/lib/ns-default.tcl`, `tcl/lib/ns-lib.tcl`, `tcl/lib/ns-mobilenode.tcl`, `tcl/lib/ns-packet.tcl`, `trace/cmu-trace.cc`, `trace/cmu-trace.h` |
