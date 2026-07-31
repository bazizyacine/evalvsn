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
#   --dbg N       ns-2 cross-layer traces: 1 (historical default) or 0.
#                 -dbg 0 removes the cause rather than the symptom: PHY_DBG_,
#                 MAC_DBG, AOMDV_QPV_DBG and IFQ_DBG are what take a log from
#                 161 KB to 1.3 GB.

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

cd "$(dirname "$(readlink -f "$0")")" || exit 1

readonly CONF="sets.conf"
DRY_RUN=0; FORCE=0; REGEN_AVI=0; QUIET_LOG=0; LIST=0; DBG=1
SETS=()

for arg in "$@"; do
  case "$arg" in
    --list)       LIST=1 ;;
    --dry-run)    DRY_RUN=1 ;;
    --force)      FORCE=1 ;;
    --regen-avi)  REGEN_AVI=1 ;;
    --quiet-log)  QUIET_LOG=1 ;;
    --dbg=*)      DBG="${arg#*=}" ;;
    -h|--help)    sed -n '2,22p' "$0"; exit 0 ;;
    -*)           die "unknown option: $arg" ;;
    *)            SETS+=("$arg") ;;
  esac
done

require_files "$CONF"

# -------------------------------------------------------------------- catalogue
declare -A RP XL MPL NN VIDEO
DECLARED=()

while read -r name rp xl mpl nn video; do
  [[ -z "$name" || "$name" == \#* ]] && continue
  DECLARED+=("$name")
  RP["$name"]="$rp"; XL["$name"]="$xl"
  MPL["$name"]="$mpl"; NN["$name"]="$nn"; VIDEO["$name"]="$video"
done < "$CONF"

if [[ $LIST -eq 1 ]]; then
  printf '%-24s %-11s %-11s %-8s %s\n' "SET" "PROTOCOL" "LABEL" "COMPUTED" "IPFILES"
  printf '%s\n' "--------------------------------------------------------------------------"
  for n in "${DECLARED[@]}"; do
    done_="no"; [[ -d "../sets/$n" ]] && done_="YES"
    ip="ok"
    [[ -f "../ipFiles/encoder_$n.dat" && -f "../ipFiles/decoder_$n.dat" ]] || ip="MISSING"
    printf '%-24s %-11s %-11s %-8s %s\n' "$n" "${RP[$n]}" "${XL[$n]}" "$done_" "$ip"
  done
  exit 0
fi

[[ ${#SETS[@]} -gt 0 ]] || die "no set named. \"$0 --list\" shows the declared sets."

for n in "${SETS[@]}"; do
  [[ -n "${RP[$n]:-}" ]] || die "unknown set in $CONF: $n"
done

# ----------------------------------------------------------------- prerequisites
log_step "Checking prerequisites"
require_cmds ffmpeg perl ns
require_files ../bin/evalvsn set.sh setGraphs.pl st-packetGenerate.sh ns2.sh evalvsn.tcl
[[ -x ../bin/evalvsn ]] || die "../bin/evalvsn is not executable."

for n in "${SETS[@]}"; do
  require_files "../ipFiles/encoder_$n.dat" "../ipFiles/decoder_$n.dat" \
                "../videos/${VIDEO[$n]}.yuv"
  if [[ -e "../sets/$n" && $FORCE -eq 0 ]]; then
    die "../sets/$n already exists. Move it, or re-run with --force to overwrite it."
  fi
done
log_info "ok - ${#SETS[@]} set(s): ${SETS[*]}"

log_info ""
log_info "Existing sets that will NOT be touched:"
for d in ../sets/*/; do
  b="$(basename "$d")"
  printf '%s\n' "${SETS[@]}" | grep -qx "$b" || log_info "  - $b"
done

[[ $DRY_RUN -eq 1 ]] && log_info $'\n--dry-run: commands are printed only, nothing is written.'

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

for name in "${SETS[@]}"; do
  out="../sets/$name"
  yuv="../videos/${VIDEO[$name]}.yuv"
  avi="../videos/${VIDEO[$name]}.avi"
  enc="../ipFiles/encoder_$name.dat"
  dec="../ipFiles/decoder_$name.dat"
  LOGFILE="$PWD/$name.log"

  log_step "SET $name  -  ${RP[$name]}, label ${XL[$name]}"
  [[ $DRY_RUN -eq 1 ]] || echo -e "Script started at $(date)\n" > "$LOGFILE"

  log_step "$name 1/5 - YUV to AVI conversion"
  if [[ -f "$avi" && $REGEN_AVI -eq 0 ]]; then
    log_info "$avi already exists, kept (--regen-avi to regenerate it)."
  else
    run_logged ffmpeg -y -s qcif -i "$yuv" "$avi"
  fi

  log_step "$name 2/5 - EvalVSN, encoding"
  run_logged ../bin/evalvsn -ip "$enc" -vf "$avi" -od "$out"

  log_step "$name 3/5 - ns-2 simulation (${RP[$name]}, ${NN[$name]} nodes)"
  run_logged bash set.sh -ip "$dec" -vf "$avi" -od "$out" \
             -mpl "${MPL[$name]}" -nn "${NN[$name]}" -rp "${RP[$name]}" -dbg "$DBG"

  log_step "$name 4/5 - EvalVSN, decoding and metrics"
  run_logged ../bin/evalvsn -ip "$dec" -vf "$avi" -od "$out"

  log_step "$name 5/5 - Plot generation"
  run_logged perl setGraphs.pl -ip "$dec" -vf "$avi" -od "$out" -xl "${XL[$name]}"

  if [[ $DRY_RUN -eq 0 ]]; then
    echo -e "Script ended at $(date)\n" >> "$LOGFILE"
    mv "$LOGFILE" "$out/"
    log_step "$name done"
    log_info "Results: $out"
    log_info "Log     : $out/$name.log  ($(du -h "$out/$name.log" | cut -f1))"
  fi
done
