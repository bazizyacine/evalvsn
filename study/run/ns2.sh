#!/bin/bash
#
# Runs an ns-2 simulation from a packet file.
#
# Usage: ns2.sh -st st-packet.dat -rt rtName -vd videoDuration -nn nodes -rp protocol
#        [-dbg 0|1] [-seed N]   seed 0 = ns-2 default sequence

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

USAGE="Usage: $0 -st st-packet.dat -rt rtName -vd videoDuration -nn number_mobile_nodes -rp routing_protocol

  -st  per-packet send trace file
  -rt  name of the reception trace file to produce
  -vd  video duration in seconds, also the simulation time
  -nn  number of mobile nodes
  -rp  routing protocol: AOMDV or AOMDV_QPV
  -dbg cross-layer traces, 1 by default (historical behaviour)"

parse_args "$USAGE" "$@"
require_args "$USAGE" st rt vd nn rp

case "${ARG[rp]}" in
  AOMDV|AOMDV_QPV) ;;
  *) die "unknown routing protocol: ${ARG[rp]} (expected AOMDV or AOMDV_QPV)" ;;
esac

require_cmds ns
require_files "${ARG[st]}" evalvsn.tcl

ns evalvsn.tcl \
  -st      "${ARG[st]}" \
  -rt      "${ARG[rt]}" \
  -simt    "${ARG[vd]}" \
  -nn      "${ARG[nn]}" \
  -rp      "${ARG[rp]}" \
  -dbg     "${ARG[dbg]:-1}" \
  -rndSeed "${ARG[seed]:-0}"
