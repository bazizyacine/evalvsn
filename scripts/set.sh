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
# All fifteen fields are named even though only four are used: the list
# documents the ipFile column order, described in README.md.
# shellcheck disable=SC2034
while read -r simId encoder resolution imagesNb videoDuration fps threshold \
              gopCoef qualityCoef priorityNb dct zoneSize rleLevel binLevel \
              rcvField _extra || [[ -n "${simId:-}" ]]; do

  if [[ -z "$simId" || "$simId" == \#* ]]; then simId=""; continue; fi

  # Only the MMPEG encoder carries an rcvFile field; other lines are skipped,
  # as in the original version.
  rcvFile=""
  [[ "$encoder" == "MMPEG" ]] && rcvFile="$rcvField"

  if [[ -n "$rcvFile" ]]; then
    stPath="${ARG[od]}/${videoName}-${simId}"
    rtName="$(basename "$rcvFile")"

    log_info "[set.sh] simId: $simId  ->  $stPath"

    require_files "$stPath/st-priority.dat"

    bash st-packetGenerate.sh -st "$stPath/st-priority.dat" -mpl "${ARG[mpl]}" \
      || die "st-packetGenerate.sh failed for $simId"

    bash ns2.sh -st st-packet.dat -rt "$rtName" -vd "$videoDuration" \
                -nn "${ARG[nn]}" -rp "${ARG[rp]}" -dbg "$DBG" \
      || die "the ns-2 simulation failed for $simId"

    if [[ "${ARG[rp]}" == "AOMDV_QPV" ]]; then
      bash ns2TrStats.sh -tr st-packet.tr
    fi

    mkdir -p "$stPath"
    for f in "${TRACES[@]}"; do
      [[ -f "$f" ]] && cp "$f" "$stPath/"
    done

    (( count++ ))
  fi

  simId=""
done < "${ARG[ip]}"

log_info "[set.sh] $count simulation(s) processed."
