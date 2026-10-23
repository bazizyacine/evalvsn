# ------------------------------------------------------------------------------
# radio-params.tcl - the radio and MAC parameters of the two stacks
# evalvsn.tcl can run, in one file.
#
# The radio is IEEE 802.15.4, the only one of the thesis (the author's rule
# of 14 September 2026: no 802.11 anywhere); -rp decides whether the EvalVSN
# cross-layer extras apply, so this file branches on $val(rp) alone.
# $val(netif) and $val(mac) name the right classes before it is sourced.
#
# The values are anchored to the radio the thesis cites - the AT86RF231 of
# the IoT-LAB M3 node (R48/R49): +3 dBm maximum output, -101 dBm
# sensitivity, the 2.4 GHz O-QPSK band - and the power-monitor constants sit
# on the -111 dBm thermal floor of its 2 MHz channel.
# ------------------------------------------------------------------------------

# The queue length the radio's real buffers justify (R48), applied when the
# scenario asked for -ifqlen radio: 16 frames - the order of the Contiki and
# TinyOS transmit queues, and a sane share of the M3's 64 KB.
if {$val(ifqlen) == "radio"} {
    set val(ifqlen) 16
    puts "Interface queue length (radio-anchored): $val(ifqlen)"
}

# The battery is the stack's, not the terrain's (R56), and both stacks live
# on the same refonte model, the author ruled: no historical carve-outs. One
# real 650 mAh / 3.7 V pack (some 8,658 J) everywhere.
if {$val(initeng) == ""} {
    set val(initeng) 8658
}

# The consumption rates are the chip's own. AT86RF231 at 3.0 V: TX +3 dBm
# 14 mA, RX_ON 12.3 mA - also the listening state, the chip has no cheaper
# idle-listening mode - SLEEP 0.02 uA.
set val(txPower)    0.042
set val(rxPower)    0.037
set val(sleepPower) 0.00000006
set val(idlePower)  0.037

$val(netif) set CPThresh_   2.0         ;# 3 dB co-channel rejection, the AT86RF231's own capture
                                        ;# figure: the ns-2 default of 10 made any transmitter at
                                        ;# 300 m corrupt a 100 m neighbour ((300/100)^2 = 9 < 10),
                                        ;# and no route survived it
$val(netif) set CSThresh_   7.94e-13    ;# -91 dBm: the AT86RF231's CCA mode 1 energy-detection
                                        ;# floor. Sensing at -106/-101 dBm heard carrier over the
                                        ;# whole grid - one collision domain, no spatial reuse; the
                                        ;# chip's own ED range restores three-cell sensing
$val(netif) set RXThresh_   7.943e-14   ;# -101 dBm, AT86RF231 sensitivity
$val(netif) set Pt_         0.002       ;# +3 dBm, AT86RF231 maximum output
$val(netif) set freq_       2.4e9       ;# 2.4 GHz O-QPSK band
$val(netif) set L_          1.0         ;# no additional system loss

# The CSMA/CA constants of Mac/802_15_4 are the standard's, compiled in
# wpan/p802_15_4const.h and left untouched: the MAC needs no line here.

if {$val(rp) == "AOMDV_QPV"} {
    # Per-second hellos are the dimensioning of a fast radio: a hundred
    # nodes of them eat a 250 kbit/s channel in collisions before any video
    # is sent, where real sensor stacks beacon in tens of seconds. Five is
    # conservative on that scale (R49, measured below).
    Agent/AOMDV_QPV set HELLO_INTERVAL_  5.0
    if {$val(hello_interval) != ""}     { Agent/AOMDV_QPV set HELLO_INTERVAL_ $val(hello_interval) } ;# study knob
    # A 1 s cap on the RREQ backoff makes a hundred nodes rebroadcast a
    # rediscovery every second - the measured ~63,000-drop storm.
    # RFC 3561's own value.
    Agent/AOMDV_QPV set MAX_RREQ_TIMEOUT_ 10.0
    if {$val(rreq_timeout) != ""}       { Agent/AOMDV_QPV set MAX_RREQ_TIMEOUT_ $val(rreq_timeout) } ;# study knob
    # Ablation knobs of the mechanisms aligned on chapter 4: the per-class
    # link cost and the HELLO queue field (default on).
    if {$val(class_cost) != ""}         { Agent/AOMDV_QPV set CLASS_COST_ $val(class_cost) }
    if {$val(hello_queue) != ""}        { Agent/AOMDV_QPV set HELLO_QUEUE_ $val(hello_queue) }
    # The learning MAC (CSMA parameters per class and link), off by default.
    if {$val(rl_access) != ""}          { Mac/802_15_4EvalVSN set RL_ACCESS_ $val(rl_access) }
    # The learning agents that replace the chapter-4 rules (d22, 10 Oct
    # 2026), each off by default: path, congestion, link-break detection,
    # discovery jitter, transmit power.
    if {$val(rl_path) != ""}            { Agent/AOMDV_QPV set RL_PATH_ $val(rl_path) }
    if {$val(rl_congestion) != ""}      { Agent/AOMDV_QPV set RL_CONGESTION_ $val(rl_congestion) }
    if {$val(rl_break) != ""}           { Agent/AOMDV_QPV set RL_BREAK_ $val(rl_break) }
    if {$val(rl_discovery) != ""}       { Agent/AOMDV_QPV set RL_DISCOVERY_ $val(rl_discovery) }
    if {$val(rl_power) != ""}           { Mac/802_15_4EvalVSN set RL_POWER_ $val(rl_power) }
    # Early discard by priority (d27): the rule (rule_discard) or its learning agent (rl_discard).
    if {$val(rule_discard) != ""}       { Agent/AOMDV_QPV set DISCARD_RULE_ $val(rule_discard) }
    if {$val(rl_discard) != ""}         { Agent/AOMDV_QPV set RL_DISCARD_ $val(rl_discard) }
    # Routes outlive the capture period (30 s at the base point, 9 s
    # under the fires of study 09), or every frame would pay the discovery
    # backoff again.
    Agent/AOMDV_QPV set ACTIVE_ROUTE_TIMEOUT_ 35.0
    $val(netif) set noise_floor_         7.94328e-15 ;# -111 dBm: thermal noise kTB over the 2 MHz channel
    $val(netif) set PowerMonitorThresh_  2.64776e-15 ;# noise_floor_ / 3, the monitor's ratio
    $val(netif) set SINRThresh_          10.0        ;# 10 dB: the -101 dBm sensitivity over the -111 dBm floor
    $val(netif) set MAX_PRIO_NB          $maxPrioNb_ ;# the PHY's own copy for its score (R58)
    $val(netif) set TxPowerAdapt_        $val(rule_power) ;# closed-loop power per neighbour on the chip's TX_PWR steps; -rule_power, default 1 (4 Oct 2026)
    $val(mac)   set MAX_PRIO_NB          $maxPrioNb_ ;# the cross-layer MAC holds the count (R58)
}
