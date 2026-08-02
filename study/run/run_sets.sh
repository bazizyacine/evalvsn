#!/bin/bash
#
# run_sets.sh - runs one or more EvalVSN experiment sets.
#
# Replaces sets.sh: sets are declared in sets.conf and named explicitly on the
# command line. No set is ever run implicitly, and an existing set is never
# overwritten without --force.
#
# Usage:
#   bash run_sets.sh --list
#   bash run_sets.sh set_test
#   bash run_sets.sh set2_GC-PN set3_GC-QC
#   bash run_sets.sh --dry-run set_test
#
# Options:
#   --list        list the declared sets and those already computed
#   --dry-run     print the commands without running anything
#   --force       allow an existing set to be overwritten
#   --regen-avi   regenerate the .avi from the .yuv even if it exists
#   --quiet-log   filter the cross-layer traces out of the log
#   --prune       delete the heavy intermediates once the metrics are extracted.
#                 Per realisation and per configuration, st-packet.tr weighs some
#                 64 MB and st-packet.nam some 14 MB; neither is read again after
#                 setGraphs.pl has run, and a ten-realisation campaign over two
#                 protocols would otherwise leave about 19 GB behind - which is
#                 how the previous 19 GB accumulated. The measurements under
#                 graphs/ are what the article and the thesis read, and they are
#                 kept.
#   --seed N      ns-2 random seed. 0 (default) keeps ns-2's own sequence, so a
#                 set recomputed without this flag reproduces bit for bit. A
#                 positive value selects one realisation; averaging over several
#                 is what separates a result from single-run noise.
#   --dbg N       ns-2 cross-layer traces: 1 (historical default) or 0.
#                 -dbg 0 removes the cause rather than the symptom: PHY_DBG_,
#                 MAC_DBG, AOMDV_QPV_DBG and IFQ_DBG are what take a log from
#                 161 KB to 1.3 GB.

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

cd "$(dirname "$(readlink -f "$0")")" || exit 1

readonly CONF="../sets.conf"
DRY_RUN=0; FORCE=0; REGEN_AVI=0; QUIET_LOG=0; LIST=0; DBG=1; SEED=0; PRUNE=0
SETS=()

for arg in "$@"; do
  case "$arg" in
    --list)       LIST=1 ;;
    --dry-run)    DRY_RUN=1 ;;
    --force)      FORCE=1 ;;
    --regen-avi)  REGEN_AVI=1 ;;
    --quiet-log)  QUIET_LOG=1 ;;
    --prune)      PRUNE=1 ;;
    --dbg=*)      DBG="${arg#*=}" ;;
    --seed=*)     SEED="${arg#*=}" ;;
    -h|--help)    sed -n '2,40p' "$0"; exit 0 ;;
    -*)           die "unknown option: $arg" ;;
    *)            SETS+=("$arg") ;;
  esac
done

require_files "$CONF"

# -------------------------------------------------------------------- catalogue
declare -A RP XL MPL NN VIDEO METRICS SIMS
DECLARED=()

# The metrics and sims columns are optional: a declaration without them behaves
# exactly as before, computing everything over a single realisation.
while read -r name rp xl mpl nn video metrics sims; do
  [[ -z "$name" || "$name" == \#* ]] && continue
  DECLARED+=("$name")
  RP["$name"]="$rp"; XL["$name"]="$xl"
  MPL["$name"]="$mpl"; NN["$name"]="$nn"; VIDEO["$name"]="$video"
  METRICS["$name"]="${metrics:-all}"; SIMS["$name"]="${sims:-1}"
done < "$CONF"

if [[ $LIST -eq 1 ]]; then
  printf '%-24s %-11s %-11s %-10s %-5s %-8s %s\n' \
         "SET" "PROTOCOL" "LABEL" "METRICS" "SIMS" "COMPUTED" "IPFILES"
  printf '%s\n' "---------------------------------------------------------------------------------------"
  for n in "${DECLARED[@]}"; do
    done_="no"; [[ -d "../sets/$n" ]] && done_="YES"
    ip="ok"
    [[ -f "../ipFiles/encoder_$n.dat" && -f "../ipFiles/decoder_$n.dat" ]] || ip="MISSING"
    printf '%-24s %-11s %-11s %-10s %-5s %-8s %s\n' \
           "$n" "${RP[$n]}" "${XL[$n]}" "${METRICS[$n]}" "${SIMS[$n]}" "$done_" "$ip"
  done
  exit 0
fi

[[ ${#SETS[@]} -gt 0 ]] || die "no set named. \"$0 --list\" shows the declared sets."

for n in "${SETS[@]}"; do
  [[ -n "${RP[$n]:-}" ]] || die "unknown set in $CONF: $n"
  [[ "${SIMS[$n]}" =~ ^[0-9]+$ && "${SIMS[$n]}" -ge 1 ]] \
    || die "$n: sims must be a positive integer, got \"${SIMS[$n]}\""
  # Validated here rather than at the plotting stage: a typo must stop the run
  # before the encoding, not after several hours of it. "-metrics X -list"
  # exits non-zero when X names something the catalogue does not hold.
  perl ../metrics/setGraphs.pl -metrics "${METRICS[$n]}" -list >/dev/null \
    || die "$n: invalid metrics \"${METRICS[$n]}\" in $CONF."
done

# ----------------------------------------------------------------- prerequisites
log_step "Checking prerequisites"
require_cmds ffmpeg perl ns
require_files ../../evalvsn/bin/evalvsn set.sh ../metrics/setGraphs.pl st-packetGenerate.sh ns2.sh evalvsn.tcl
[[ -x ../../evalvsn/bin/evalvsn ]] || die "../../evalvsn/bin/evalvsn is not executable."

for n in "${SETS[@]}"; do
  require_files "../ipFiles/encoder_$n.dat" "../ipFiles/decoder_$n.dat" \
                "../../evalvsn/videos/${VIDEO[$n]}.yuv"
  if [[ -e "../sets/$n" && $FORCE -eq 0 ]]; then
    die "../sets/$n already exists. Move it, or re-run with --force to overwrite it."
  fi
done
log_info "ok - ${#SETS[@]} set(s): ${SETS[*]}"

log_info ""
log_info "Existing sets that will NOT be touched:"
untouched=0
for d in ../sets/*/; do
  # An unmatched glob comes back as the literal pattern, which printed a bare
  # "*" as though a set by that name existed.
  [[ -d "$d" ]] || continue
  b="$(basename "$d")"
  printf '%s\n' "${SETS[@]}" | grep -qx "$b" || { log_info "  - $b"; untouched=1; }
done
[[ $untouched -eq 1 ]] || log_info "  (none)"

[[ $DRY_RUN -eq 1 ]] && log_info $'\n--dry-run: commands are printed only, nothing is written.'

# Said once, up front, because a set runs for hours and is usually launched in
# the background: without this the only trace of progress is the terminal that
# started it.
if [[ $DRY_RUN -eq 0 ]]; then
  log_info ""
  log_info "Progress is written to ../sets/<set>/PROGRESS.txt as it goes. Watch it with:"
  for n in "${SETS[@]}"; do
    log_info "  tail -f ../sets/$n/PROGRESS.txt"
  done
fi

# -------------------------------------------------------------------- execution
run_logged() {
  echo "\$ $*"
  [[ $DRY_RUN -eq 1 ]] && return 0
  if [[ $QUIET_LOG -eq 1 ]]; then
    # Drops the cross-layer traces, which account for most of the volume
    # (1.3 GB for a twelve-second AOMDV_QPV simulation). Errors and application
    # messages are kept.
    "$@" 2>&1 | grep -vE "$CROSSLAYER_TRACE_PATTERN" >> "$LOGFILE"
    local rc=${PIPESTATUS[0]}
  else
    "$@" >> "$LOGFILE" 2>&1
    local rc=$?
  fi
  if [[ $rc -ne 0 ]]; then
    echo "FAILED (code $rc) - last 20 lines of $LOGFILE:" >&2
    tail -20 "$LOGFILE" >&2
    exit $rc
  fi
}

# Seeds are derived from the set name and the realisation index, never drawn at
# run time: a campaign that cannot be repeated is not a measurement. cksum gives
# a stable number across machines; the modulo keeps it inside the range ns-random
# accepts, and the +1 keeps it away from 0, which means "use ns-2's own sequence".
seed_for() {
  local n
  n=$(printf '%s-%s' "$1" "$2" | cksum | cut -d' ' -f1)
  echo $(( n % 900000 + 1 ))
}

for name in "${SETS[@]}"; do
  out="../sets/$name"
  yuv="../../evalvsn/videos/${VIDEO[$name]}.yuv"
  avi="../../evalvsn/videos/${VIDEO[$name]}.avi"
  enc="../ipFiles/encoder_$name.dat"
  dec="../ipFiles/decoder_$name.dat"
  sims="${SIMS[$name]}"
  LOGFILE="$PWD/$name.log"

  # The set directory and its PROGRESS.txt are created before any work starts,
  # not at the first harvest. A set takes hours; without this the directory does
  # not exist for the first half-hour and nothing distinguishes a run that is
  # working from one that failed at once. PROGRESS.txt is the file to tail.
  if [[ $DRY_RUN -eq 0 ]]; then
    mkdir -p "$out"
    PROGRESS_LOG="$out/PROGRESS.txt"
    : >"$PROGRESS_LOG"
    log_to_progress "Set $name started on $(date '+%d/%m/%Y at %H:%M')"
    log_to_progress "${RP[$name]}, ${sims} realisation(s), metrics=${METRICS[$name]}"
    log_to_progress "Raw command output: $out/$name.log"
    log_to_progress ""
  fi

  log_step "SET $name  -  ${RP[$name]}, label ${XL[$name]}, ${sims} realisation(s)"
  [[ $DRY_RUN -eq 1 ]] || echo -e "Script started at $(date)\n" > "$LOGFILE"

  log_step "$name 1/4 - YUV to AVI conversion"
  if [[ -f "$avi" && $REGEN_AVI -eq 0 ]]; then
    log_info "$avi already exists, kept (--regen-avi to regenerate it)."
  else
    run_logged ffmpeg -y -s qcif -i "$yuv" "$avi"
  fi

  # The encoding depends only on the video and the encoder parameters - not on
  # the seed, not on the routing protocol. Running it once and sharing it across
  # realisations is what takes a ten-realisation campaign from about twenty
  # hours to about four; it is the single largest cost in the chain.
  log_step "$name 2/4 - EvalVSN, encoding (once, shared by all realisations)"
  run_logged ../../evalvsn/bin/evalvsn -ip "$enc" -vf "$avi" -od "$out/encoding"

  for ((i = 1; i <= sims; i++)); do
    r=$(printf 'r%02d' "$i")
    rdir="$out/$r"
    # An explicit --seed applies to every realisation, which is how a single run
    # is reproduced exactly; otherwise each realisation gets its own derived seed.
    if [[ "$SEED" != "0" ]]; then seed="$SEED"; else seed="$(seed_for "$name" "$i")"; fi

    log_step "$name 3/4 - realisation $i/$sims ($r, seed $seed)"

    # Hard links, not copies: the encoding weighs some 22 MB per configuration,
    # so ten realisations of a twelve-configuration set would duplicate about
    # 2.6 GB for nothing. Decoding writes only into sim/decodedImages (video.cc,
    # the rcvFile branch), so the shared files are never written through.
    log_info "  linking the shared encoding into $r (no second encoding)"
    if [[ $DRY_RUN -eq 0 ]]; then
      mkdir -p "$rdir"
      cp -al "$out/encoding/." "$rdir/" \
        || die "could not link the encoding into $rdir"
    else
      echo "\$ cp -al $out/encoding/. $rdir/"
    fi

    log_info "  ns-2 transmission"
    run_logged bash set.sh -ip "$dec" -vf "$avi" -od "$rdir" \
               -mpl "${MPL[$name]}" -nn "${NN[$name]}" -rp "${RP[$name]}" -dbg "$DBG" \
               -seed "$seed"

    # Same binary as the encoding step, so the log line looks alike; what makes
    # this a decoding is the ipFile. decoder_*.dat carries a trailing rcvFile
    # field that encoder_*.dat does not, and video.cc branches on it: without it
    # EvalVSN writes referenceImages/, with it it writes sim/decodedImages/.
    log_info "  EvalVSN decoding (decoder ipFile - this does NOT re-encode)"
    run_logged ../../evalvsn/bin/evalvsn -ip "$dec" -vf "$avi" -od "$rdir"
  done

  log_step "$name 4/4 - Metrics and plots (${METRICS[$name]}, over $sims realisation(s))"
  run_logged perl ../metrics/setGraphs.pl -ip "$dec" -vf "$avi" -od "$out" -xl "${XL[$name]}" \
             -metrics "${METRICS[$name]}" -runs "$sims"

  # Pruning comes after the metrics are extracted, never before: what is deleted
  # here can only be regenerated by running the set again.
  if [[ $DRY_RUN -eq 0 ]]; then
    before="$(du -sh "$out" | cut -f1)"
    if [[ $PRUNE -eq 1 ]]; then
      log_step "$name - pruning the intermediates ($before before)"
      find "$out" -type f \( -name '*.tr' -o -name '*.nam' \) -delete
      find "$out" -type d \( -name 'allImages' -o -name 'capturedImages' \
                          -o -name 'decodedImages' \) -exec rm -rf {} +
      log_info "  $before -> $(du -sh "$out" | cut -f1); graphs/ kept"
    else
      log_info ""
      log_info "  $out weighs $before. Most of it is ns-2 traces and decoded"
      log_info "  frames, read only by the step that has just finished."
      log_info "  Re-run with --prune, or delete them once the figures are drawn."
    fi
  fi

  if [[ $DRY_RUN -eq 0 ]]; then
    echo -e "Script ended at $(date)\n" >> "$LOGFILE"
    mv "$LOGFILE" "$out/"
    log_step "$name done"
    log_info "Results: $out"
    log_info "Log     : $out/$name.log  ($(du -h "$out/$name.log" | cut -f1))"
  fi
done
