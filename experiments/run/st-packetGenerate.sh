#!/bin/bash
#
# Turns the encoder's packets, one per line of st-priority.dat since d53
# (each cut on whole code words, at most the payload), into st-packet.dat in
# the current directory, with their send times. A line longer than the
# payload - an encoding made before d53 - is still cut into slices.
#
# Usage: st-packetGenerate.sh -st st-priority.dat -mpl max_payload
#        [-period P] [-ipt T] [-detect D]

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

USAGE="Usage: $0 -st st-priority.dat -mpl max_payload [-period P] [-ipt T] [-detect D]

  -st     the st-priority.dat file produced by the encoding step
  -mpl    maximum packet payload, in bytes
  -period inter-capture period in seconds; 0 (default) keeps the encoder's
          own send times - the historical video schedule
  -ipt    inter-packet time in seconds, the sensor's transmit pacing; 0
          (default) keeps the historical same-instant bursts
  -detect detection date in seconds, when the first capture leaves (0)"

parse_args "$USAGE" "$@"
require_args "$USAGE" st mpl
require_files "${ARG[st]}"

[[ "${ARG[mpl]}" =~ ^[0-9]+$ ]] || die "-mpl must be a positive integer: ${ARG[mpl]}"
(( ARG[mpl] > 0 ))              || die "-mpl must be strictly positive."

readonly OUT="st-packet.dat"

# The banner declares the format and its version. Readers check it instead of
# counting columns or seeking a fixed number of bytes into the file, which is
# how both readers used to find the first record - and why adding a column
# used to corrupt a whole campaign in silence.
readonly TRACE_FORMAT_VERSION=1
printf '#!evsn-trace version=%d fields=8 payload=hex\n' "$TRACE_FORMAT_VERSION" > "$OUT"
printf '#sTime\tseqNb\tpSize\tframeNb\tframeType\tprioNb\toffset\tpayload\n' >> "$OUT"

seqNb=-1

# st-priority.dat is tab-separated: read with IFS=$'\t' splits reliably, even
# when a field contains spaces. The sixth field is the hexadecimal payload of
# the priority level, which is what this script used to discard.
while IFS=$'\t' read -r sendTime frameNb frameType dataPriority dataSize data; do
  [[ -z "$sendTime" || "$sendTime" == \#* ]] && continue

  # Offsets and sizes count bytes; the payload is hexadecimal, so it takes two
  # characters per byte. Every slice below therefore doubles.
  offset=0
  while (( offset + ARG[mpl] < dataSize )); do
    (( seqNb++ ))
    printf '%s\t%d\t%d\t%s\t%s\t%s\t%d\t%s\n' \
      "$sendTime" "$seqNb" "${ARG[mpl]}" "$frameNb" "$frameType" "$dataPriority" "$offset" \
      "${data:$(( offset * 2 )):$(( ARG[mpl] * 2 ))}" >> "$OUT"
    (( offset += ARG[mpl] ))
  done

  # The tail packet takes what is left. Its size is derived from the slice
  # rather than from dataSize - offset: a level whose bit stream ends on a
  # half byte leaves an odd number of hexadecimal characters, and deriving the
  # size the other way round would declare one byte less than it carries.
  (( seqNb++ ))
  tail="${data:$(( offset * 2 ))}"
  printf '%s\t%d\t%d\t%s\t%s\t%s\t%d\t%s\n' \
    "$sendTime" "$seqNb" "$(( (${#tail} + 1) / 2 ))" "$frameNb" "$frameType" "$dataPriority" "$offset" \
    "$tail" >> "$OUT"
done < "${ARG[st]}"

# The capture calendar (R25/R40, tranche 5). The encoder stamps every packet
# of a frame at the same instant - a playback, not a sensor. With a period,
# frame k leaves at detect + (k-1)*period; with an inter-packet time, packets
# drain sequentially and never regress: each send time is the later of its
# frame's capture date and the previous send plus the pacing - a transmit
# queue, in one line of arithmetic. Defaults of zero reproduce the historical
# file byte for byte.
period="${ARG[period]:-0}"
ipt="${ARG[ipt]:-0}"
detect="${ARG[detect]:-0}"

if [[ "$period" != "0" || "$ipt" != "0" ]]; then
  awk -F'\t' -v OFS='\t' -v period="$period" -v ipt="$ipt" -v detect="$detect" '
    /^#/ { print; next }
    {
      base = (period > 0) ? detect + ($4 - 1) * period : $1
      t = (NR > 2 && prev + ipt > base) ? prev + ipt : base
      $1 = sprintf("%.6f", t)
      prev = t
      print
    }' "$OUT" > "$OUT.paced" && mv "$OUT.paced" "$OUT"
fi

log_info "$OUT: $(( seqNb + 1 )) packets."
