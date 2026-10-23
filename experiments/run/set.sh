#!/bin/bash
#
# For each simulation declared in a parameter file, runs the packetisation,
# the ns-2 transmission and the trace collection.
#
# Usage:
#   set.sh -ip input_parameters_file -vf video_file -od output_directory \
#          -mpl max_payload -nn number_mobile_nodes -rp routing_protocol [-dbg 0|1]

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

cd "$(dirname "$(readlink -f "$0")")" || exit 1

USAGE="Usage: $0 -ip input_parameters_file -vf video_file -od output_directory -mpl max_payload -nn number_mobile_nodes -rp routing_protocol [-dbg 0|1]

  -ip   parameter file, one line per simulation
  -vf   video, AVI format
  -od   set directory
  -mpl  maximum packet payload, in bytes
  -nn   number of mobile nodes
  -rp   AOMDV or AOMDV_QPV
  -capture period:ipt[:detect] in seconds - the capture calendar and packet
        pacing of the source sensor; 0:0 (default) keeps the historical
        video schedule and its same-instant bursts
  -fire t0:v[:x:y] - forest-fire front: ignition at (x, y) metres at t0
        seconds, radial spread at v m/s, nodes destroyed as it passes;
        0 (default) runs the static scenario unchanged
  -prop tworay (default) or shadowing[:exp:std] - the propagation model
  -standby period:size - bi-regime scalar flow, 0 (default) none
  -knobs k=v[,k=v] - stack switches forwarded to ns-2 through ns2.sh;
          the header of
          evalvsn.tcl lists every switch and its default; 0 (default) none
  -dbg  cross-layer traces, 1 by default (historical behaviour)"

parse_args "$USAGE" "$@"
require_args "$USAGE" ip vf od mpl nn rp
require_files "${ARG[ip]}" "${ARG[vf]}"

DBG="${ARG[dbg]:-1}"
videoName="$(basename "${ARG[vf]}" | cut -d. -f1)"

# Traces produced in the current directory by ns-2 and by the packetisation,
# then moved into the simulation directory. The list is explicit: the original
# "cp *.dat *.tr *.nam" swept up the whole directory, including leftovers from a
# previous run, and failed when one of the patterns matched nothing.
TRACES=(st-packet.dat st-packet.tr st-packet.nam ns2_st-packet.dat rt-packet.dat)
[[ "${ARG[rp]}" == "AOMDV_QPV" ]] && TRACES+=(aomdv_qpv.tr)

count=0

# The "|| [[ -n \$simId ]]" guard processes the last line even without a final
# newline: none of the repository ipFiles has one, and without it the last
# simulation of every set would be silently skipped.
#
# The source family - resolution, frame count, duration - is read from the
# video itself since R39, so the simulation time comes from ffprobe on the
# AVI rather than from a column. All remaining fields are named even though
# only two are used: the list documents the ipFile column order, described
# in README.md. rcvFile is no longer a column either - the decoding pass is
# selected by -decode on the evalvsn command line - so every MMPEG line is a
# simulation to run.
# IEEE 802.15.4 carries 102 bytes of MAC payload (127-byte PHY frame, 25 of
# overhead), and ns-2 adds its 20-byte IP header to the application
# payload: 82 bytes is the ceiling here, and the study's 74 leaves the room
# a real UDP header takes. Refused up front: a frame too long never leaves
# the MAC, the interface queue fills behind it and every packet is dropped
# as "IFQ NEW", a 0 % delivery that looks like a result (test bench, 14
# September 2026).
if [[ "${ARG[mpl]}" -gt 82 ]]; then
  die "-mpl ${ARG[mpl]} does not fit an IEEE 802.15.4 frame: 82 bytes at most (102 of MAC payload minus the 20-byte IP header)"
fi

require_cmds ffprobe
videoDuration=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "${ARG[vf]}")
videoDuration=$(printf '%.0f' "$videoDuration")
[[ "$videoDuration" =~ ^[0-9]+$ && "$videoDuration" -gt 0 ]] \
  || die "could not derive the video duration from ${ARG[vf]}"

# shellcheck disable=SC2034
while read -r simId encoder fps threshold \
              gopCoef qualityCoef priorityNb dctZoneSize rleLevel binLevel \
              _extra || [[ -n "${simId:-}" ]]; do

  if [[ -z "$simId" || "$simId" == \#* ]]; then simId=""; continue; fi

  # Two grammars share the file: the MMPEG line and, since tranche 3b-2,
  # the JPEG line of the Pham witness (simId JPEG FPS Threshold QF MSS);
  # the columns beyond the second are named for MMPEG and read positionally,
  # and none of them is used here, so both run the same steps.
  if [[ "$encoder" == "MMPEG" || "$encoder" == "JPEG" ]]; then
    stPath="${ARG[od]}/${videoName}-${simId}"
    rtName="rt-packet.dat"

    log_info "[set.sh] simId: $simId  ->  $stPath"

    require_files "$stPath/st-priority.dat"

    IFS=':' read -r capPeriod capIpt capDetect <<< "${ARG[capture]:-0:0:0}"

    bash st-packetGenerate.sh -st "$stPath/st-priority.dat" -mpl "${ARG[mpl]}" \
      -period "${capPeriod:-0}" -ipt "${capIpt:-0}" -detect "${capDetect:-0}" \
      || die "st-packetGenerate.sh failed for $simId"

    # With a capture calendar the simulation lasts as long as the calendar,
    # not as long as the source video: the last send time, rounded up, plus
    # two seconds for the tail to drain.
    simTime="$videoDuration"
    if [[ "${capPeriod:-0}" != "0" || "${capIpt:-0}" != "0" ]]; then
      simTime=$(awk -F'\t' '!/^#/{t=$1} END{printf "%d", int(t) + 3}' st-packet.dat)
    fi

    # A stale fire schedule from an earlier run in this directory must not
    # be swept into this one.
    rm -f fire-nodes.dat

    bash ns2.sh -st st-packet.dat -rt "$rtName" -vd "$simTime" \
                -nn "${ARG[nn]}" -rp "${ARG[rp]}" \
                -dist "${ARG[dist]:-100}" -fire "${ARG[fire]:-0}" \
                -prop "${ARG[prop]:-tworay}" -standby "${ARG[standby]:-0}" \
                -knobs "${ARG[knobs]:-0}" \
                -dbg "$DBG" -seed "${ARG[seed]:-0}" \
      || die "the ns-2 simulation failed for $simId"

    if [[ "${ARG[rp]}" == "AOMDV_QPV" ]]; then
      bash ../metrics/ns2TrStats.sh -tr st-packet.tr
    fi

    mkdir -p "$stPath"
    for f in "${TRACES[@]}"; do
      [[ -f "$f" ]] && cp "$f" "$stPath/"
    done
    [[ -f fire-nodes.dat ]] && cp fire-nodes.dat "$stPath/"

    (( count++ ))
  fi

  simId=""
done < "${ARG[ip]}"

log_info "[set.sh] $count simulation(s) processed."
