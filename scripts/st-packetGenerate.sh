#!/bin/bash
#
# Splits the encoded stream, described per priority level in st-priority.dat,
# into packets of bounded payload. Produces st-packet.dat in the current
# directory.
#
# Usage: st-packetGenerate.sh -st st-priority.dat -mpl max_payload

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

USAGE="Usage: $0 -st st-priority.dat -mpl max_payload

  -st   the st-priority.dat file produced by the encoding step
  -mpl  maximum packet payload, in bytes"

parse_args "$USAGE" "$@"
require_args "$USAGE" st mpl
require_files "${ARG[st]}"

[[ "${ARG[mpl]}" =~ ^[0-9]+$ ]] || die "-mpl must be a positive integer: ${ARG[mpl]}"
(( ARG[mpl] > 0 ))              || die "-mpl must be strictly positive."

readonly OUT="st-packet.dat"
printf '#sTime\tseqNb\tpSize\tframeNb\tframeType\tprioNb\toffset\n' > "$OUT"

seqNb=-1

# st-priority.dat is tab-separated: read with IFS=$'\t' splits reliably, even
# when a field contains spaces.
while IFS=$'\t' read -r sendTime frameNb frameType dataPriority dataSize _rest; do
  [[ -z "$sendTime" || "$sendTime" == \#* ]] && continue

  offset=0
  while (( offset + ARG[mpl] < dataSize )); do
    (( seqNb++ ))
    printf '%s\t%d\t%d\t%s\t%s\t%s\t%d\n' \
      "$sendTime" "$seqNb" "${ARG[mpl]}" "$frameNb" "$frameType" "$dataPriority" "$offset" >> "$OUT"
    (( offset += ARG[mpl] ))
  done

  (( seqNb++ ))
  printf '%s\t%d\t%d\t%s\t%s\t%s\t%d\n' \
    "$sendTime" "$seqNb" "$(( dataSize - offset ))" "$frameNb" "$frameType" "$dataPriority" "$offset" >> "$OUT"
done < "${ARG[st]}"

log_info "$OUT: $(( seqNb + 1 )) packets."
